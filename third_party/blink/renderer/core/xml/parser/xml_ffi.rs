// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod entities;

use std::io::Cursor;
use xml::{
    attribute::OwnedAttribute,
    common::{Position, TextPosition},
    namespace::{Namespace, NS_XMLNS_URI, NS_XML_URI},
    reader::{
        EventReader as XmlEventReader,
        XmlEvent::{
            CData, Characters, Comment, Doctype, EndDocument, EndElement, ProcessingInstruction,
            StartDocument, StartElement, Whitespace,
        },
    },
    Encoding, ParserConfig,
};

use crate::ffi::{AttributeNameValue, StandaloneInfo, XmlCallbacks};
use entities::{HTML5_MAP, LAT1_MAP, SPECIAL_MAP, SYMBOL_MAP};
use std::pin::Pin;

struct XmlReadState<'a> {
    event_reader: XmlEventReader<Cursor<Vec<u8>>>,
    error_details: Option<xml::reader::Error>,
    last_event_position: Option<TextPosition>,
    parser_callbacks: Pin<&'a mut XmlCallbacks>,
    namespace_stack: Vec<Namespace>,
    seen_first_event: bool,
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
        .allow_multiple_root_elements(false);
    return XmlEventReader::new_with_config(cursor, parser_config);
}

fn create_read_state<'a>(callbacks: Pin<&mut XmlCallbacks>) -> Box<XmlReadState<'_>> {
    let event_reader = create_reader();
    Box::new(XmlReadState {
        event_reader,
        error_details: None,
        last_event_position: None,
        parser_callbacks: callbacks,
        namespace_stack: Vec::new(),
        seen_first_event: false,
    })
}

fn append_to_source(read_state: &mut XmlReadState, content: &[u8]) {
    let buffer = read_state.event_reader.source_mut().get_mut();
    buffer.extend_from_slice(content);
}

struct AttributesIterator<'a> {
    attributes: std::slice::Iter<'a, OwnedAttribute>,
}

struct NamespacesIterator<'a> {
    namespaces: Box<dyn Iterator<Item = (&'a str, &'a str)> + 'a>,
}

fn new_namespaces(existing: &Namespace, new: &Namespace, seen_first_event: bool) -> Namespace {
    let mut result = Namespace::empty();
    for (new_prefix, new_uri) in new.iter() {
        // Don't add the first empty default namespace, as the parser synthesizes it and
        // it likely did not come from the input document.
        // See: https://github.com/kornelski/xml-rs/issues/48
        if existing.get(new_prefix).is_none_or(|uri| uri != new_uri)
            && (seen_first_event || new_prefix != "" || new_uri != "")
        {
            result.put(new_prefix, new_uri);
        }
    }
    result
}

fn process_next_event(read_state: &mut XmlReadState) {
    let event_result = read_state.event_reader.next();
    match event_result {
        Ok(event) => {
            read_state.last_event_position = Some(read_state.event_reader.position());
            match event {
                StartDocument { version, encoding, standalone } => {
                    let version: &str = version.as_str();

                    let buffer = read_state.event_reader.source().get_ref();
                    let xml_declaration_view = read_state.last_event_position.and_then(|pos| {
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
                                // <?xml instruction present, but standalone not specified
                                return StandaloneInfo::kStandaloneUnspecified;
                            }
                            StandaloneInfo::kNoXmlDeclaration
                        });
                    let encoding = xml_declaration_view
                        .and_then(|bytes| {
                            if bytes.windows(8).any(|window| window == b"encoding") {
                                Some(encoding)
                            } else {
                                None
                            }
                        })
                        .unwrap_or_default();
                    read_state
                        .parser_callbacks
                        .as_mut()
                        .StartDocument(version, &encoding, standalone);
                }
                ProcessingInstruction { name, data } => {
                    let data = data.unwrap_or_default();
                    let data = data.trim_start();
                    read_state.parser_callbacks.as_mut().ProcessingInstruction(&name, &data);
                }
                StartElement { name, attributes, namespace } => {
                    let local_name: &str = &name.local_name;
                    let has_prefix = name.prefix.is_some();
                    let prefix: &str = &name.prefix.unwrap_or_default();
                    let has_ns = name.namespace.is_some();
                    let ns: &str = &name.namespace.unwrap_or_default();

                    let new_namespaces = new_namespaces(
                        &read_state.namespace_stack.last().unwrap_or(&Namespace::empty()),
                        &namespace,
                        read_state.seen_first_event,
                    );

                    read_state.seen_first_event = true;

                    read_state.namespace_stack.push(namespace);
                    let mut attributes = AttributesIterator { attributes: attributes.iter() };
                    let mut new_namespaces =
                        NamespacesIterator { namespaces: Box::new(new_namespaces.iter()) };
                    read_state.parser_callbacks.as_mut().StartElementNs(
                        local_name,
                        has_prefix,
                        prefix,
                        has_ns,
                        ns,
                        &mut attributes,
                        &mut new_namespaces,
                    );
                }
                EndElement { name } => {
                    read_state.namespace_stack.pop();

                    let local_name: &str = &name.local_name;
                    let prefix: &str = &name.prefix.unwrap_or_default();
                    let ns: &str = &name.namespace.unwrap_or_default();
                    read_state.parser_callbacks.as_mut().EndElementNs(local_name, prefix, ns);
                }
                Characters(characters) => {
                    read_state.parser_callbacks.as_mut().Characters(&characters);
                }
                Whitespace(whitespace_characters) => {
                    read_state.parser_callbacks.as_mut().Characters(&whitespace_characters);
                }
                Comment(comment) => {
                    read_state.parser_callbacks.as_mut().Comment(&comment);
                }
                CData(data) => {
                    read_state.parser_callbacks.as_mut().CData(&data);
                }
                EndDocument => {
                    read_state.parser_callbacks.as_mut().EndDocument();
                }
                Doctype { .. } => {
                    // It's safe to unwrap here. `doctype_ids()` after a `Doctype`
                    // event only fails if there's no doctype name, which would have
                    // resulted in a parser error earlier.
                    // See: https://github.com/kornelski/xml-rs/blob/main/src/reader/parser.rs#L162
                    let ids = read_state.event_reader.doctype_ids().unwrap();
                    read_state.parser_callbacks.as_mut().DocType(
                        ids.name(),
                        ids.public_id().unwrap_or(""),
                        ids.system_id().unwrap_or(""),
                    );
                }
            }
        }
        Err(failure) => {
            read_state.error_details = Some(failure.clone());
            return;
        }
    }
}

fn try_get_last_event_position(read_state: &XmlReadState, row: &mut u64, col: &mut u64) -> bool {
    if let Some(position) = read_state.last_event_position {
        *row = position.row;
        *col = position.column;
        return true;
    }
    false
}

fn add_xhtml_entities(read_state: &mut XmlReadState) {
    for map in [LAT1_MAP, SYMBOL_MAP, SPECIAL_MAP, HTML5_MAP].iter() {
        read_state
            .event_reader
            .add_entities(map.iter().copied())
            .expect("Adding built-in entities should not cause an error.");
    }
}

fn try_get_error_details(
    read_state: &XmlReadState,
    msg: &mut String,
    row: &mut u64,
    col: &mut u64,
) -> bool {
    if let Some(error_details) = &read_state.error_details {
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
    read_state.error_details.is_some()
}

fn attributes_next<'a>(
    attributes: &mut AttributesIterator<'a>,
    local_name: &mut String,
    ns: &mut String,
    prefix: &mut String,
    value: &'a mut String,
) -> bool {
    if let Some(attribute) = attributes.attributes.next() {
        let name = &attribute.name;
        *local_name = name.local_name.clone();
        *ns = name.namespace.clone().unwrap_or_default();
        *prefix = name.prefix.clone().unwrap_or_default();
        *value = attribute.value.clone();
        return true;
    }
    false
}

fn namespaces_next<'a>(
    namespaces_iterator: &'a mut NamespacesIterator,
    prefix: &mut String,
    uri: &mut String,
) -> bool {
    loop {
        if let Some(namespace) = namespaces_iterator.namespaces.next() {
            // TODO(drott): Why does the library generate these default ones?
            // TODO(drott): Why do we see an empty namespace here for
            // fast/dom/attribute-namespaces-get-set.html and XML like:
            // <root xmlns:foo=\"http://www.example.com\" attr=\"test2\" foo:attr=\"test\" />
            // and virtual/rust-xml/fast/xmlhttprequest/xmlhttprequest-get.xhtml
            // Filed as: https://github.com/kornelski/xml-rs/issues/50

            // Letting the empty namespace and empty URL pass through here
            // is important to reset the default namespace to none.
            if (namespace.0 == "xml" && namespace.1 == NS_XML_URI)
                || (namespace.0 == "xmlns" && namespace.1 == NS_XMLNS_URI)
            {
                continue;
            }

            *prefix = namespace.0.to_string();
            *uri = namespace.1.to_string();
            return true;
        } else {
            break;
        }
    }
    false
}

fn parse_attributes<'a>(
    attributes_string: &'a [u8],
    success: &mut bool,
) -> Vec<AttributeNameValue> {
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
        // CXX Rust::Str and &str do not allow a distinction between null and empty
        // strings, so one way to convey that to the C++ side is to carry an
        // extra boolean - which we require to be able to distinguish between a
        // null and an empty namespace URI.
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
    }

    extern "Rust" {
        type XmlReadState<'a>;
        type AttributesIterator<'a>;
        type NamespacesIterator<'a>;
        unsafe fn create_read_state<'a>(
            callbacks: Pin<&'a mut XmlCallbacks>,
        ) -> Box<XmlReadState<'a>>;
        fn append_to_source(read_state: &mut XmlReadState, content: &[u8]);
        fn process_next_event(read_state: &mut XmlReadState);
        fn try_get_last_event_position(
            read_state: &XmlReadState,
            row: &mut u64,
            col: &mut u64,
        ) -> bool;
        fn add_xhtml_entities(read_state: &mut XmlReadState);
        fn try_get_error_details(
            read_state: &XmlReadState,
            msg: &mut String,
            row: &mut u64,
            col: &mut u64,
        ) -> bool;
        fn saw_error(read_state: &XmlReadState) -> bool;

        unsafe fn parse_attributes<'a>(
            attributes_string: &'a [u8],
            success: &mut bool,
        ) -> Vec<AttributeNameValue>;

        unsafe fn attributes_next<'a>(
            attributes: &mut AttributesIterator<'a>,
            local_name: &mut String,
            ns: &mut String,
            prefix: &mut String,
            value: &'a mut String,
        ) -> bool;
        unsafe fn namespaces_next<'a>(
            namespaces_iterator: &mut NamespacesIterator<'a>,
            prefix: &mut String,
            uri: &mut String,
        ) -> bool;
    }
}
