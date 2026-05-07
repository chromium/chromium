// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace=data_decoder::xml::ffi)]
pub mod ffi {
    unsafe extern "C++" {
        include!("services/data_decoder/public/cpp/xml_dom.h");
        include!("services/data_decoder/xml/dom_builder.h");

        #[namespace = "data_decoder::xml"]
        type Node;

        fn create_element(local_name: &str, prefix: &str) -> UniquePtr<Node>;
        fn create_text_node(text_content: &str) -> UniquePtr<Node>;
        fn create_cdata_node(text_content: &str) -> UniquePtr<Node>;
        fn set_attribute(element: Pin<&mut Node>, local_name: &str, prefix: &str, value: &str);
        fn set_namespace(element: Pin<&mut Node>, prefix: &str, uri: &str);
        fn add_child(parent: Pin<&mut Node>, child: UniquePtr<Node>);
    }

    extern "Rust" {
        // TODO(dcheng): decide if we really need to handle both bytes and
        // strings and/or where the conversion should happen.
        fn decode_xml_bytes(xml: &[u8], err: Pin<&mut CxxString>) -> UniquePtr<Node>;
        fn decode_xml_str(xml: &str, mut err: Pin<&mut CxxString>) -> UniquePtr<Node>;
    }
}

use crate::{decode_xml_bytes, decode_xml_str};
