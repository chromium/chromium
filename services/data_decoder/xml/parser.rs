// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![forbid(unsafe_code)]

use crate::cxx::ffi::Node;
use cxx::{CxxString, UniquePtr};
use std::fmt::Write;
use std::pin::Pin;
use xml::attribute::OwnedAttribute;
use xml::namespace::Namespace;
use xml::reader::{EventReader, XmlEvent};

const MAX_DEPTH: usize = 200;

fn populate_node(
    node: &mut UniquePtr<Node>,
    attributes: Vec<OwnedAttribute>,
    namespace: &Namespace,
    parent_namespace: Option<&Namespace>,
) {
    for attr in attributes {
        let prefix = attr.name.prefix.as_deref().unwrap_or("");
        crate::cxx::ffi::set_attribute(
            node.as_mut().unwrap(),
            &attr.name.local_name,
            prefix,
            &attr.value,
        );
    }
    for (prefix, uri) in namespace {
        // Per https://www.w3.org/TR/xml-names/#ns-decl, "xml" is reserved and may be declared, but
        // must be bound to ttp://www.w3.org/XML/1998/namespace. xml-rs already checks
        // that, so no need to validate that here.
        // "xmlns" is also reserved but must not be declared; this is also enforced by
        // xml-rs.
        if prefix == "xml" || prefix == "xmlns" {
            continue;
        }
        // For compatibility with the original C++ parser, which omits this on the root
        // element if not otherwise specified.
        if prefix.is_empty() && uri.is_empty() && parent_namespace.is_none() {
            continue;
        }
        // `namespace` includes declarations from the parent (and transitively, ancestor
        // nodes); avoid duplicating inherited declarations into all descendant
        // nodes.
        if parent_namespace.and_then(|ns| ns.get(prefix)) == Some(uri) {
            continue;
        }
        crate::cxx::ffi::set_namespace(node.as_mut().unwrap(), prefix, uri);
    }
}

pub fn decode_xml_bytes(xml: &[u8], err: Pin<&mut CxxString>) -> UniquePtr<Node> {
    let Ok(xml) = str::from_utf8(xml) else {
        return UniquePtr::null();
    };
    decode_xml_str(xml, err)
}

pub fn decode_xml_str(xml: &str, mut err: Pin<&mut CxxString>) -> UniquePtr<Node> {
    let parser = EventReader::from_str(xml);
    let mut stack: Vec<(UniquePtr<Node>, Namespace)> = Vec::new();
    let mut root: UniquePtr<Node> = UniquePtr::null();

    for e in parser {
        match e {
            Ok(XmlEvent::StartElement { name, attributes, namespace }) => {
                if !root.is_null() || stack.len() >= MAX_DEPTH {
                    return UniquePtr::null();
                }

                let prefix = name.prefix.as_deref().unwrap_or("");
                let mut node = crate::cxx::ffi::create_element(&name.local_name, prefix);

                let parent_namespace = stack.last().map(|(_, ns)| ns);
                populate_node(&mut node, attributes, &namespace, parent_namespace);

                stack.push((node, namespace));
            }
            Ok(XmlEvent::EndElement { .. }) => {
                let (child, _) = match stack.pop() {
                    Some(x) => x,
                    None => return UniquePtr::null(),
                };
                if let Some((parent, _)) = stack.last_mut() {
                    crate::cxx::ffi::add_child(parent.as_mut().unwrap(), child);
                } else {
                    root = child;
                }
            }
            Ok(XmlEvent::Characters(data)) => {
                if let Some((parent, _)) = stack.last_mut() {
                    let text_node = crate::cxx::ffi::create_text_node(&data);
                    crate::cxx::ffi::add_child(parent.as_mut().unwrap(), text_node);
                } else if !data.trim().is_empty() {
                    return UniquePtr::null();
                }
            }
            Ok(XmlEvent::CData(data)) => {
                if let Some((parent, _)) = stack.last_mut() {
                    let cdata_node = crate::cxx::ffi::create_cdata_node(&data);
                    crate::cxx::ffi::add_child(parent.as_mut().unwrap(), cdata_node);
                } else {
                    return UniquePtr::null();
                }
            }
            Ok(XmlEvent::Whitespace(_))
            | Ok(XmlEvent::Comment(_))
            | Ok(XmlEvent::ProcessingInstruction { .. })
            | Ok(XmlEvent::StartDocument { .. })
            | Ok(XmlEvent::EndDocument)
            | Ok(XmlEvent::Doctype { .. }) => {}
            Err(e) => {
                write!(&mut err, "{}", e).unwrap();
                return UniquePtr::null();
            }
        }
    }

    if !stack.is_empty() {
        return UniquePtr::null();
    }

    root
}
