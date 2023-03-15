// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests for system + encoding that use a real Mojo implementation.

extern crate test_util as util;

use mojo::bindings::mojom::{MojomInterface, MojomInterfaceRecv, MojomInterfaceSend};
use mojo::system::message_pipe;
use mojo::system::{Handle, HandleSignals};

use std::thread;

// TODO(crbug.com/1274864): share this in a better way. These targets will need
// to be refactored extensively anyway. Especially once we are generating mojom
// bindings instead of using these old hard-coded ones.
#[path = "../encoding/mojom_validation.rs"]
mod mojom_validation;

use mojom_validation::*;

// Tests basic client and server interaction over a thread
#[test]
fn send_and_recv() {
    util::init();

    let (endpt0, endpt1) = message_pipe::create().unwrap();
    // Client and server handles
    let client = IntegrationTestInterfaceClient::new(endpt0);
    let server = IntegrationTestInterfaceServer::with_version(endpt1, 0);

    // Client thread
    let handle = thread::spawn(move || {
        // Send request
        client
            .send_request(
                5,
                IntegrationTestInterfaceMethod0Request { param0: BasicStruct { a: -1 } },
            )
            .unwrap();
        // Wait for response
        client.pipe().wait(HandleSignals::READABLE);
        // Decode response
        let (req_id, options) = client.recv_response().unwrap();
        assert_eq!(req_id, 5);
        match options {
            IntegrationTestInterfaceResponseOption::IntegrationTestInterfaceMethod0(msg) => {
                assert_eq!(msg.param0, vec![1, 2, 3]);
            }
        }
    });
    // Wait for request
    server.pipe().wait(HandleSignals::READABLE);
    // Decode request
    let (req_id, options) = server.recv_response().unwrap();
    assert_eq!(req_id, 5);
    match options {
        IntegrationTestInterfaceRequestOption::IntegrationTestInterfaceMethod0(msg) => {
            assert_eq!(msg.param0.a, -1);
        }
    }
    // Send response
    server
        .send_request(5, IntegrationTestInterfaceMethod0Response { param0: vec![1, 2, 3] })
        .unwrap();
    let _ = handle.join();
}
