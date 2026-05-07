// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![forbid(unsafe_code)]

use crate::cxx::ffi::Node;
use cxx::UniquePtr;
use xml::attribute::OwnedAttribute;
use xml::reader::{EventReader, XmlEvent};

const MAX_DEPTH: usize = 200;

fn populate_node(node: &mut UniquePtr<Node>, attributes: Vec<OwnedAttribute>) {
    for attr in attributes {
        crate::cxx::ffi::set_attribute(node.as_mut().unwrap(), &attr.name.local_name, &attr.value);
    }
}

pub fn decode_xml_bytes(xml: &[u8]) -> UniquePtr<Node> {
    let Ok(xml) = str::from_utf8(xml) else {
        return UniquePtr::null();
    };
    decode_xml_str(xml)
}

pub fn decode_xml_str(xml: &str) -> UniquePtr<Node> {
    let parser = EventReader::from_str(xml);
    let mut stack: Vec<UniquePtr<Node>> = Vec::new();
    let mut root: UniquePtr<Node> = UniquePtr::null();

    for e in parser {
        match e {
            Ok(XmlEvent::StartElement { name, attributes, .. }) => {
                if !root.is_null() || stack.len() >= MAX_DEPTH {
                    return UniquePtr::null();
                }

                let mut node = crate::cxx::ffi::create_element(&name.local_name);
                populate_node(&mut node, attributes);
                stack.push(node);
            }
            Ok(XmlEvent::EndElement { .. }) => {
                let child = match stack.pop() {
                    Some(x) => x,
                    None => return UniquePtr::null(),
                };
                if let Some(parent) = stack.last_mut() {
                    crate::cxx::ffi::add_child(parent.as_mut().unwrap(), child);
                } else {
                    root = child;
                }
            }
            Ok(XmlEvent::Characters(data)) => {
                if let Some(parent) = stack.last_mut() {
                    let text_node = crate::cxx::ffi::create_text_node(&data);
                    crate::cxx::ffi::add_child(parent.as_mut().unwrap(), text_node);
                } else if !data.trim().is_empty() {
                    return UniquePtr::null();
                }
            }
            Ok(XmlEvent::CData(data)) => {
                if let Some(parent) = stack.last_mut() {
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
            Err(_) => return UniquePtr::null(),
        }
    }

    if !stack.is_empty() {
        return UniquePtr::null();
    }

    root
}
