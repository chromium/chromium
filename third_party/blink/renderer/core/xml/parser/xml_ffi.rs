// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod entities;

use std::io::Cursor;
use xml::{
    attribute::OwnedAttribute,
    common::{Position, TextPosition},
    reader::{
        ErrorKind, EventReader as XmlEventReader, ImmutableEntitiesError,
        XmlEvent::{
            self, CData, Characters, Comment, Doctype, EndDocument, EndElement,
            ProcessingInstruction, StartDocument, StartElement, Whitespace,
        },
    },
    Encoding, ParserConfig,
};

use crate::ffi::{AttributeNameValue, AttributeView, NamespaceView, StandaloneInfo, XmlCallbacks};
use entities::{HTML5_MAP, LAT1_MAP, SPECIAL_MAP, SYMBOL_MAP};
use std::cell::{Cell, RefCell};
use std::pin::Pin;

// `XmlReadState` is held by `XMLDocumentParserRs` and passed across FFI as a
// shared `&XmlReadState` reference. While `dispatch_event()` is on the
// stack, synchronous C++ parser callbacks can re-enter `xml_ffi` with the same
// `read_state_` (for example, `DocType()` calls `add_xhtml_entities()`, and
// `StartElementNs()` calls `try_get_last_event_position()`). Using interior
// mutability (`RefCell` / `Cell`) and dropping the `event_reader` borrow before
// invoking `parser_callbacks` avoids creating aliasing `&mut` references across
// FFI while allowing callbacks to safely access `event_reader` or
// `last_event_position`.
struct XmlReadState<'a> {
    event_reader: RefCell<XmlEventReader<Cursor<Vec<u8>>>>,
    // The result of the last `read_next_event()`, which `dispatch_event()`
    // passes to the callbacks by reference. It is freed by the next
    // `read_next_event()`, which `XMLDocumentParserRs::ProcessEvents()` runs
    // with the PartitionAlloc Scheduler-Loop Quarantine disabled, so that the
    // event's many small allocations don't go through the quarantine. The
    // quarantine is only disabled around `read_next_event()`, which only runs
    // safe Rust code and doesn't call into Blink.
    event: RefCell<Option<xml::reader::Result<XmlEvent>>>,
    error_details: RefCell<Option<xml::reader::Error>>,
    last_event_position: Cell<Option<TextPosition>>,
    parser_callbacks: RefCell<Pin<&'a mut XmlCallbacks>>,
}

fn create_reader() -> XmlEventReader<Cursor<Vec<u8>>> {
    // Replacing Cursor<Vec<u8>> with a VecDeque<u8> is currently not feasible
    // because we access the underlying buffer in StartDocument to more
    // specifically determine document properties that the current parser API
    // does not give us.
    let cursor = Cursor::new(Vec::new());
    let parser_config = ParserConfig::new()
        .override_encoding(Some(Encoding::Utf8))
        .ignore_invalid_encoding_declarations(true)
        .ignore_comments(false)
        .ignore_end_of_stream(true)
        .allow_multiple_root_elements(false)
        // `StartElement`'s `namespace` isn't used, and computing it is
        // expensive.
        .start_element_namespace(false);
    XmlEventReader::new_with_config(cursor, parser_config)
}

fn create_read_state(callbacks: Pin<&mut XmlCallbacks>) -> Box<XmlReadState<'_>> {
    let event_reader = create_reader();
    Box::new(XmlReadState {
        event_reader: RefCell::new(event_reader),
        event: RefCell::new(None),
        error_details: RefCell::new(None),
        last_event_position: Cell::new(None),
        parser_callbacks: RefCell::new(callbacks),
    })
}

fn append_to_source(read_state: &XmlReadState, content: &[u8]) {
    let mut reader = read_state.event_reader.borrow_mut();
    let buffer = reader.source_mut().get_mut();
    // When the encoding is set fixed on the xml Rust crate, it does not do its
    // own BOM sniffing, hence removing a possible BOM here.
    // The byte sequence [0xEF, 0xBB, 0xBF] is the UTF-8 representation of the
    // Unicode Byte Order Mark (U+FEFF). Because Blink converts input strings
    // (WTF::String) to UTF-8 before passing them to this FFI layer, any leading
    // BOM is always encoded as UTF-8, and other BOM shapes (such as UTF-16
    // or UTF-32) do not need to be handled here.
    let content = if buffer.is_empty() {
        content.strip_prefix(&[0xEF, 0xBB, 0xBF]).unwrap_or(content)
    } else {
        content
    };
    buffer.extend_from_slice(content);
}

struct AttributesIterator<'a> {
    attributes: std::slice::Iter<'a, OwnedAttribute>,
}

struct NamespacesIterator {
    namespaces: std::vec::IntoIter<(String, String)>,
}

// Reads the next event into `read_state.event`, after freeing the previous
// one. This doesn't call into Blink, so the caller can exclude it from the
// PartitionAlloc Scheduler-Loop Quarantine.
fn read_next_event(read_state: &XmlReadState) {
    // Blink is only called through `parser_callbacks` (the attribute and
    // namespace views only exist inside those calls), which borrows it
    // mutably. Holding a borrow here makes any such call panic when debug
    // assertions (DCHECKs) are enabled.
    #[cfg(debug_assertions)]
    let _no_callbacks = read_state.parser_callbacks.borrow();
    let mut event = read_state.event.borrow_mut();
    *event = None;
    let mut reader = read_state.event_reader.borrow_mut();
    let result = reader.next();
    if result.is_ok() {
        read_state.last_event_position.set(Some(reader.position()));
    }
    *event = Some(result);
}

// Passes the event read by `read_next_event()` to the parser callbacks.
fn dispatch_event(read_state: &XmlReadState) {
    let event = read_state.event.borrow();
    let Some(event_result) = event.as_ref() else {
        return;
    };
    match event_result {
        Ok(event) => match event {
            StartDocument { version, encoding, standalone } => {
                let version: &str = version.as_str();

                let (standalone, encoding) = {
                    let reader = read_state.event_reader.borrow();
                    let buffer = reader.source().get_ref();
                    let xml_declaration_view =
                        read_state.last_event_position.get().and_then(|pos| {
                            let start_index = pos.column as usize;
                            if buffer[start_index..].starts_with(b"<?xml ") {
                                buffer[start_index..].iter().position(|&c| c == b'>').map(
                                    |end_offset| &buffer[start_index..start_index + end_offset + 1],
                                )
                            } else {
                                None
                            }
                        });

                    let standalone = standalone
                        .map(|is_standalone| {
                            if is_standalone {
                                StandaloneInfo::kStandaloneYes
                            } else {
                                StandaloneInfo::kStandaloneNo
                            }
                        })
                        .unwrap_or_else(|| {
                            if xml_declaration_view.is_some() {
                                // <?xml instruction present, but standalone not
                                // specified
                                return StandaloneInfo::kStandaloneUnspecified;
                            }
                            StandaloneInfo::kNoXmlDeclaration
                        });
                    let encoding = xml_declaration_view
                        .and_then(|bytes| {
                            if bytes.windows(8).any(|window| window == b"encoding") {
                                Some(encoding.as_str())
                            } else {
                                None
                            }
                        })
                        .unwrap_or_default();
                    (standalone, encoding)
                };
                read_state
                    .parser_callbacks
                    .borrow_mut()
                    .as_mut()
                    .StartDocument(version, encoding, standalone);
            }
            ProcessingInstruction { name, data } => {
                let data = data.as_deref().unwrap_or_default();
                let data = data.trim_start();
                read_state.parser_callbacks.borrow_mut().as_mut().ProcessingInstruction(name, data);
            }
            StartElement { name, attributes, .. } => {
                let local_name: &str = &name.local_name;
                let has_prefix = name.prefix.is_some();
                let prefix: &str = name.prefix.as_deref().unwrap_or_default();
                let has_ns = name.namespace.is_some();
                let ns: &str = name.namespace.as_deref().unwrap_or_default();

                // Avoids holding a borrow of `read_state` when calling back
                // into C++ below. Since this would require cloning, also
                // preprocess `declared_namespaces()` to reduce the number of
                // temporary string constructions.
                let mut namespaces = NamespacesIterator {
                    namespaces: read_state
                        .event_reader
                        .borrow()
                        .declared_namespaces()
                        .map(|declared| {
                            declared
                                .iter()
                                .map(|(prefix, uri)| {
                                    let q_name = if prefix.is_empty() {
                                        String::new()
                                    } else {
                                        ["xmlns:", prefix].concat()
                                    };
                                    (q_name, uri.to_owned())
                                })
                                .collect::<Vec<_>>()
                                .into_iter()
                        })
                        .unwrap_or_default(),
                };
                // The attributes are borrowed from `read_state.event`, so they
                // are freed with it in the next `read_next_event()`.
                let mut attributes = AttributesIterator { attributes: attributes.iter() };
                read_state.parser_callbacks.borrow_mut().as_mut().StartElementNs(
                    local_name,
                    has_prefix,
                    prefix,
                    has_ns,
                    ns,
                    &mut attributes,
                    &mut namespaces,
                );
            }
            EndElement { name } => {
                let local_name: &str = &name.local_name;
                let prefix: &str = name.prefix.as_deref().unwrap_or_default();
                let ns: &str = name.namespace.as_deref().unwrap_or_default();
                read_state
                    .parser_callbacks
                    .borrow_mut()
                    .as_mut()
                    .EndElementNs(local_name, prefix, ns);
            }
            Characters(characters) => {
                read_state.parser_callbacks.borrow_mut().as_mut().Characters(characters);
            }
            Whitespace(whitespace_characters) => {
                read_state.parser_callbacks.borrow_mut().as_mut().Characters(whitespace_characters);
            }
            Comment(comment) => {
                read_state.parser_callbacks.borrow_mut().as_mut().Comment(comment);
            }
            CData(data) => {
                read_state.parser_callbacks.borrow_mut().as_mut().CData(data);
            }
            EndDocument => {
                read_state.parser_callbacks.borrow_mut().as_mut().EndDocument();
            }
            Doctype { .. } => {
                // Clone the doctype IDs so the `event_reader` borrow is
                // released before invoking the C++ `DocType` callback, which
                // may call `add_xhtml_entities()` and mutably borrow
                // `event_reader`.
                let (name, public_id, system_id) = {
                    let reader = read_state.event_reader.borrow();
                    // It's safe to unwrap here. `doctype_ids()` after a
                    // `Doctype` event only fails if there's
                    // no doctype name, which would have
                    // resulted in a parser error earlier.
                    // See: https://github.com/kornelski/xml-rs/blob/main/src/reader/parser.rs#L162
                    let ids = reader.doctype_ids().unwrap();
                    (
                        ids.name().to_owned(),
                        ids.public_id().unwrap_or("").to_owned(),
                        ids.system_id().unwrap_or("").to_owned(),
                    )
                };
                read_state
                    .parser_callbacks
                    .borrow_mut()
                    .as_mut()
                    .DocType(&name, &public_id, &system_id);
            }
        },
        Err(failure) => {
            *read_state.error_details.borrow_mut() = Some(failure.clone());
        }
    }
}

fn try_get_last_event_position(read_state: &XmlReadState, row: &mut u64, col: &mut u64) -> bool {
    if let Some(position) = read_state.last_event_position.get() {
        *row = position.row;
        *col = position.column;
        return true;
    }
    false
}

fn add_xhtml_entities(read_state: &XmlReadState) {
    let mut reader = read_state.event_reader.borrow_mut();
    for map in [LAT1_MAP, SYMBOL_MAP, SPECIAL_MAP, HTML5_MAP].iter() {
        match reader.add_entities(map.iter().copied()) {
            Ok(()) | Err(ImmutableEntitiesError::StandaloneDocument) => {}
            Err(e) => panic!("Adding built-in entities should not cause an error: {e}"),
        }
    }
}

fn try_get_error_details(
    read_state: &XmlReadState,
    msg: &mut String,
    row: &mut u64,
    col: &mut u64,
) -> bool {
    if let Some(error_details) = read_state.error_details.borrow().as_ref() {
        let error_string = error_details.to_string();
        // Remove the "row:col " prefix from the error message.
        // The row and col are passed separately.
        *msg = error_string.find(" ").map_or_else(
            || error_string.clone(),
            |space_pos| error_string[space_pos + 1..].to_string(),
        );
        *row = error_details.position().row;
        *col = error_details.position().column;
        return true;
    }
    false
}

fn saw_error(read_state: &XmlReadState) -> bool {
    read_state.error_details.borrow().is_some()
}

fn is_error_resumable(read_state: &XmlReadState) -> bool {
    // See XML crate's parser.rs handle_eof() handling.
    // which returns SyntaxError::UnbalancedRootElement on
    // temporary end of read stream situations.
    matches!(read_state.error_details.borrow().as_ref().map(|error| error.kind()),
             Some(ErrorKind::Syntax(msg)) if
             msg.starts_with("Unexpected end of stream"))
}

fn reset_error(read_state: &XmlReadState) {
    *read_state.error_details.borrow_mut() = None;
}

fn attributes_next(
    attributes: &mut AttributesIterator,
    mut attribute_view: Pin<&mut AttributeView>,
) -> bool {
    if let Some(attribute) = attributes.attributes.next() {
        let name = &attribute.name;
        // Prepare qualified name according to expectations of QualifiedName
        // parser, concatenate here to avoid extra string conversions and
        // allocations on the Blink C++ side.
        match &name.prefix {
            Some(prefix) => {
                let q_name = [prefix, ":", &name.local_name].concat();
                attribute_view.as_mut().Populate(
                    &q_name,
                    name.namespace.as_deref().unwrap_or_default(),
                    &attribute.value,
                );
            }
            None => {
                attribute_view.as_mut().Populate(
                    &name.local_name,
                    name.namespace.as_deref().unwrap_or_default(),
                    &attribute.value,
                );
            }
        }
        return true;
    }
    false
}

fn namespaces_next(
    namespaces_iterator: &mut NamespacesIterator,
    mut namespace_view: Pin<&mut NamespaceView>,
) -> bool {
    if let Some((q_name, uri)) = namespaces_iterator.namespaces.next() {
        namespace_view.as_mut().Populate(&q_name, &uri);
        return true;
    }
    false
}

fn parse_attributes(attributes_string: &[u8], success: &mut bool) -> Vec<AttributeNameValue> {
    let mut reader = create_reader();

    let buffer = reader.source_mut().get_mut();
    buffer.extend_from_slice(b"<?xml version=\"1.0\"?><attrs ");
    buffer.extend_from_slice(attributes_string);
    buffer.extend_from_slice(b" />");

    loop {
        let event_result = reader.next();
        match event_result {
            Ok(StartDocument { .. }) => {}
            Ok(StartElement { attributes, .. }) => {
                *success = true;
                let mut ret_attributes: Vec<AttributeNameValue> =
                    Vec::with_capacity(attributes.len());
                attributes.iter().for_each(|attribute| {
                    let q_name = if let Some(prefix) = &attribute.name.prefix {
                        format!("{}:{}", prefix, attribute.name.local_name)
                    } else {
                        attribute.name.local_name.clone()
                    };
                    let name_value = AttributeNameValue { q_name, value: attribute.value.clone() };
                    ret_attributes.push(name_value);
                });
                return ret_attributes;
            }
            _ => {
                *success = false;
                break;
            }
        }
    }
    Vec::new()
}

#[cxx::bridge(namespace = "xml_ffi")]
mod ffi {

    enum StandaloneInfo {
        kStandaloneUnspecified = -2,
        kNoXmlDeclaration = -1,
        kStandaloneNo = 0,
        kStandaloneYes = 1,
    }

    struct AttributeNameValue {
        pub q_name: String,
        pub value: String,
    }

    unsafe extern "C++" {
        include!("third_party/blink/renderer/core/xml/parser/xml_ffi_callbacks.h");
        type XmlCallbacks;
        fn StartDocument(
            self: Pin<&mut XmlCallbacks>,
            version: &str,
            encoding: &str,
            standalone: StandaloneInfo,
        );
        fn ProcessingInstruction(self: Pin<&mut XmlCallbacks>, name: &str, data: &str);
        // CXX Rust::Str and &str do not allow a distinction between null and
        // empty strings, so one way to convey that to the C++ side is
        // to carry an extra boolean - which we require to be able to
        // distinguish between a null and an empty namespace URI.
        #[allow(clippy::too_many_arguments)]
        fn StartElementNs(
            self: Pin<&mut XmlCallbacks>,
            local_name: &str,
            has_prefix: bool,
            prefix: &str,
            has_ns: bool,
            ns: &str,
            attributes: &mut AttributesIterator,
            namespaces: &mut NamespacesIterator,
        );
        fn EndElementNs(self: Pin<&mut XmlCallbacks>, local_name: &str, prefix: &str, ns: &str);
        fn Characters(self: Pin<&mut XmlCallbacks>, characters: &str);
        fn CData(self: Pin<&mut XmlCallbacks>, data: &str);
        fn Comment(self: Pin<&mut XmlCallbacks>, comment: &str);
        fn DocType(self: Pin<&mut XmlCallbacks>, name: &str, public_id: &str, system_id: &str);
        fn EndDocument(self: Pin<&mut XmlCallbacks>);

        type AttributeView;
        fn Populate(self: Pin<&mut AttributeView>, q_name: &str, attr_ns: &str, value: &str);

        type NamespaceView;
        fn Populate(self: Pin<&mut NamespaceView>, q_name: &str, uri: &str);
    }

    extern "Rust" {
        type XmlReadState<'a>;
        type AttributesIterator<'a>;
        type NamespacesIterator;

        /// # Safety
        ///
        /// Marked unsafe because functions with explicit lifetimes in a cxx
        /// bridge require unsafe qualification.
        unsafe fn create_read_state<'a>(
            callbacks: Pin<&'a mut XmlCallbacks>,
        ) -> Box<XmlReadState<'a>>;
        fn append_to_source(read_state: &XmlReadState, content: &[u8]);
        fn read_next_event(read_state: &XmlReadState);
        fn dispatch_event(read_state: &XmlReadState);
        fn try_get_last_event_position(
            read_state: &XmlReadState,
            row: &mut u64,
            col: &mut u64,
        ) -> bool;
        fn add_xhtml_entities(read_state: &XmlReadState);
        fn try_get_error_details(
            read_state: &XmlReadState,
            msg: &mut String,
            row: &mut u64,
            col: &mut u64,
        ) -> bool;
        fn saw_error(read_state: &XmlReadState) -> bool;
        fn is_error_resumable(read_state: &XmlReadState) -> bool;
        fn reset_error(read_state: &XmlReadState);

        fn parse_attributes(
            attributes_string: &[u8],
            success: &mut bool,
        ) -> Vec<AttributeNameValue>;

        fn attributes_next(
            attributes: &mut AttributesIterator,
            attribute_view: Pin<&mut AttributeView>,
        ) -> bool;

        fn namespaces_next(
            namespaces_iterator: &mut NamespacesIterator,
            namespace_view: Pin<&mut NamespaceView>,
        ) -> bool;
    }
}
