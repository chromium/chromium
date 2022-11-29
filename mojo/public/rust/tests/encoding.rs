// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests encoding and decoding functionality in the bindings package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::bindings::encoding::Context;
use mojo::bindings::message::MessageHeader;
use mojo::bindings::mojom::{MojomInterface, MojomPointer, MojomStruct};

use mojo::system;
use mojo::system::Handle;

use std::collections::HashMap;

use crate::util;
use crate::util::mojom_validation::*;

/// This macro is a wrapper for the tests! macro as it takes advantage of the
/// shared code between tests.
///
/// Given a test name, it will generate a test function. In this test function
/// we perform the following steps:
///   1. Decode the header of the validation input.
///   2. Verify the decoded header is what we expect.
///   3. Decode the payload of the validation input.
///   4. Verify the decoded payload is what we expect.
///   5. Take the decoded payload and re-encode.
///   6. Decode this re-encoded payload.
///   7. Verify the re-decoded payload is what we expect.
///
/// Each test should sufficiently verify the operation of the encoding and
/// decoding frameworks as we first verify the decoder works correctly on the
/// "golden files", then verify that the encoder works by encoding the decoded
/// output, and decoding that once again.
macro_rules! encoding_tests {
    ($($name:ident { MessageHeader => $header_cls:expr, $req_type:ident => $cls:expr } )*) => {
        tests! {
            $(
            fn $name() {
                let data = include_str!(concat!("../../interfaces/bindings/tests/data/validation/",
                                                stringify!($name),
                                                ".data"));
                match util::parse_validation_test(data) {
                    Ok((mut data, num_handles)) => {
                        let mut mock_handles = Vec::with_capacity(num_handles);
                        for _ in 0..num_handles {
                            mock_handles.push(unsafe { system::acquire(0) });
                        }
                        println!("{}: Decoding header", stringify!($name));
                        let header = MessageHeader::deserialize(&mut data[..], Vec::new()).expect("Should not error");
                        let ctxt: Context = Default::default();
                        let header_size = header.serialized_size(&ctxt);
                        let header_cls = $header_cls;
                        println!("{}: Verifying decoded header", stringify!($name));
                        header_cls(header);
                        let payload_buffer = &mut data[header_size..];
                        let cls = $cls;
                        println!("{}: Decoding payload", stringify!($name));
                        let decoded_payload = $req_type::deserialize(payload_buffer, mock_handles).expect("Should not error");
                        println!("{}: Verifying decoded payload", stringify!($name));
                        cls(&decoded_payload);
                        println!("{}: Re-encoding payload", stringify!($name));
                        let (mut encoded_payload, handles) = decoded_payload.auto_serialize();
                        println!("{}: Decoding payload again", stringify!($name));
                        let redecoded_payload = $req_type::deserialize(&mut encoded_payload[..], handles).expect("Should not error");
                        println!("{}: Verifying decoded payload again", stringify!($name));
                        cls(&redecoded_payload);
                    },
                    Err(msg) => panic!("Error: {}", msg),
                }
            }
            )*
        }
    }
}

encoding_tests! {
    conformance_mthd0_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 0);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod0Request => {
            |payload: &ConformanceTestInterfaceMethod0Request| {
                assert_eq!(payload.param0, -1.0);
            }
        }
    }
    conformance_mthd1_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 1);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod1Request => {
            |payload: &ConformanceTestInterfaceMethod1Request| {
                assert_eq!(payload.param0.i, 1234);
            }
        }
    }
    conformance_mthd2_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 2);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod2Request => {
            |payload: &ConformanceTestInterfaceMethod2Request| {
                assert_eq!(payload.param0.struct_a.i, 12345);
                assert_eq!(payload.param1.i, 67890);
            }
        }
    }
    conformance_mthd3_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 3);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod3Request => {
            |payload: &ConformanceTestInterfaceMethod3Request| {
                assert_eq!(payload.param0, vec![true, false, true, false,
                                                true, false, true, false,
                                                true, true,  true, true]);
            }
        }
    }
    conformance_mthd4_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 4);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod4Request => {
            |payload: &ConformanceTestInterfaceMethod4Request| {
                assert_eq!(payload.param0.data, vec![0, 1, 2]);
                assert_eq!(payload.param1, vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
            }
        }
    }
    conformance_mthd5_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 5);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod5Request => {
            |payload: &ConformanceTestInterfaceMethod5Request| {
                assert_eq!(payload.param0.struct_d.message_pipes.len(), 2);
                for h in payload.param0.struct_d.message_pipes.iter() {
                    assert_eq!(h.get_native_handle(), 0);
                }
                assert_eq!(payload.param0.data_pipe_consumer.get_native_handle(), 0);
                assert_eq!(payload.param1.get_native_handle(), 0);
            }
        }
    }
    conformance_mthd6_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 6);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod6Request => {
            |payload: &ConformanceTestInterfaceMethod6Request| {
                assert_eq!(payload.param0, vec![vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9]]);
            }
        }
    }
    conformance_mthd7_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 7);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod7Request => {
            |payload: &ConformanceTestInterfaceMethod7Request| {
                assert_eq!(payload.param0.fixed_size_array, [0, 1, 2]);
                assert_eq!(payload.param1, [None, Some([0, 1, 2])]);
            }
        }
    }
    conformance_mthd8_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 8);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod8Request => {
            |payload: &ConformanceTestInterfaceMethod8Request| {
                assert_eq!(payload.param0,
                    vec![None, Some(vec![String::from_utf8(vec![0, 1, 2, 3, 4]).unwrap()]), None]);
            }
        }
    }
    conformance_mthd9_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 9);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod9Request => {
            |payload: &ConformanceTestInterfaceMethod9Request| {
                assert!(payload.param0.is_some());
                if let Some(ref v) = payload.param0 {
                    assert_eq!(v.len(), 2);
                    assert_eq!(v[0].len(), 2);
                    assert_eq!(v[1].len(), 3);
                    assert!(v[0][0].is_some());
                    assert!(v[0][1].is_none());
                    assert!(v[1][0].is_some());
                    assert!(v[1][1].is_none());
                    assert!(v[1][2].is_some());
                    assert_eq!(v[0][0].as_ref().unwrap().get_native_handle(), 0);
                    assert_eq!(v[1][0].as_ref().unwrap().get_native_handle(), 0);
                    assert_eq!(v[1][2].as_ref().unwrap().get_native_handle(), 0);
                }
            }
        }
    }
    conformance_mthd9_good_null_array {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 9);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod9Request => {
            |payload: &ConformanceTestInterfaceMethod9Request| {
                assert!(payload.param0.is_none());
            }
        }
    }
    conformance_mthd10_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 10);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod10Request => {
            |payload: &ConformanceTestInterfaceMethod10Request| {
                let mut map = HashMap::with_capacity(2);
                map.insert(String::from_utf8(vec![0, 1, 2, 3, 4]).unwrap(), 1);
                map.insert(String::from_utf8(vec![5, 6, 7, 8, 9]).unwrap(), 2);
                assert_eq!(payload.param0, map);
            }
        }
    }
    // Non-unique keys are strange...
    // Right now, we handle them by silently overwriting.
    // Maybe this will be an error in the future.
    // The insert calls below reflect the duplicate keys that the
    // test provides, and an example as to how overwriting happens.
    conformance_mthd10_good_non_unique_keys {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 10);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod10Request => {
            |payload: &ConformanceTestInterfaceMethod10Request| {
                let mut map = HashMap::with_capacity(2);
                map.insert(String::from_utf8(vec![0, 1, 2, 3, 4]).unwrap(), 1);
                map.insert(String::from_utf8(vec![0, 1, 2, 3, 4]).unwrap(), 2);
                assert_eq!(payload.param0, map);
            }
        }
    }
    conformance_mthd11_good_version0 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, false);
                assert!(payload.param0.struct_a.is_none());
                assert!(payload.param0.str.is_none());
            }
        }
    }
    conformance_mthd11_good_version1 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, false);
                assert!(payload.param0.struct_a.is_none());
                assert!(payload.param0.str.is_none());
            }
        }
    }
    conformance_mthd11_good_version2 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, false);
                assert!(payload.param0.struct_a.is_none());
                assert!(payload.param0.str.is_none());
            }
        }
    }
    conformance_mthd11_good_version3 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, true);
                assert!(payload.param0.struct_a.is_none());
                assert_eq!(payload.param0.str, Some(String::from_utf8(vec![0, 1]).unwrap()));
            }
        }
    }
    conformance_mthd11_good_version_newer_than_known_1 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, true);
                assert!(payload.param0.struct_a.is_none());
                assert_eq!(payload.param0.str, Some(String::from_utf8(vec![0, 1]).unwrap()));
            }
        }
    }
    conformance_mthd11_good_version_newer_than_known_2 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 11);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod11Request => {
            |payload: &ConformanceTestInterfaceMethod11Request| {
                assert_eq!(payload.param0.i, 123);
                assert_eq!(payload.param0.b, true);
                assert!(payload.param0.struct_a.is_none());
                assert_eq!(payload.param0.str, Some(String::from_utf8(vec![0, 1]).unwrap()));
            }
        }
    }
    conformance_mthd13_good_1 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 13);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod13Request => {
            |payload: &ConformanceTestInterfaceMethod13Request| {
                assert!(payload.param0.is_none());
                assert_eq!(payload.param1, 65535);
                assert!(payload.param2.is_none());
            }
        }
    }
    conformance_mthd13_good_2 {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 0);
                assert_eq!(header.name, 13);
                assert_eq!(header.flags, 0);
            }
        },
        ConformanceTestInterfaceMethod13Request => {
            |payload: &ConformanceTestInterfaceMethod13Request| {
                assert!(payload.param0.is_some());
                assert_eq!(payload.param0.as_ref().unwrap().pipe().get_native_handle(), 0);
                assert_eq!(payload.param1, 65535);
                assert!(payload.param2.is_some());
                assert_eq!(payload.param2.as_ref().unwrap().pipe().get_native_handle(), 0);
            }
        }
    }
    integration_intf_rqst_mthd0_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 1);
                assert_eq!(header.name, 0);
                assert_eq!(header.flags, 1);
                assert_eq!(header.request_id, 7);
            }
        },
        IntegrationTestInterfaceMethod0Request => {
            |payload: &IntegrationTestInterfaceMethod0Request| {
                assert_eq!(payload.param0.a, -1);
            }
        }
    }
    integration_intf_resp_mthd0_good {
        MessageHeader => {
            |header: MessageHeader| {
                assert_eq!(header.version, 1);
                assert_eq!(header.name, 0);
                assert_eq!(header.flags, 2);
                assert_eq!(header.request_id, 1);
            }
        },
        IntegrationTestInterfaceMethod0Response => {
            |payload: &IntegrationTestInterfaceMethod0Response| {
                assert_eq!(payload.param0, vec![0]);
            }
        }
    }

    // Tests with missing data:
    //
    // conformance_mthd14_good_1 {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::a(ref val) => assert_eq!(*val, 54),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_array_in_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::d(ref val) => assert_eq!(*val, Some(vec![0, 1, 2])),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_map_in_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             let mut map = HashMap::with_capacity(2);
    //             map.insert(String::from_utf8(vec![0, 1, 2, 3, 4]).unwrap(), 1);
    //             map.insert(String::from_utf8(vec![5, 6, 7, 8, 9]).unwrap(), 2);
    //             match payload.param0 {
    //                 UnionA::e(ref val) => assert_eq!(*val, Some(map)),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_nested_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::f(ref val) => {
    //                     assert!(val.is_some());
    //                     let inner = val.as_ref().unwrap();
    //                     match *inner {
    //                         UnionB::b(inner_val) => assert_eq!(inner_val, 10),
    //                         _ => panic!("Incorrect inner union variant! Tag found: {}", inner.get_tag()),
    //                     }
    //                 },
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_null_array_in_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::d(ref val) => assert_eq!(*val, None),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_null_map_in_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::e(ref val) => assert_eq!(*val, None),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_struct_in_union {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::c(ref val) => {
    //                     let struct_val = val.as_ref().unwrap();
    //                     assert_eq!(struct_val.i, 20);
    //                 },
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd14_good_unknown_union_tag {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 14);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod14Request => {
    //         |payload: &ConformanceTestInterfaceMethod14Request| {
    //             match payload.param0 {
    //                 UnionA::_Unknown(ref val) => assert_eq!(*val, 54),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", payload.param0.get_tag()),
    //             }
    //         }
    //     }
    // }
    // conformance_mthd15_good_union_in_array {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 15);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod15Request => {
    //         |payload: &ConformanceTestInterfaceMethod15Request| {
    //             assert_eq!(payload.param0.a, true);
    //             assert_eq!(payload.param0.b, 22);
    //             assert!(payload.param0.c.is_none());
    //             assert!(payload.param0.d.is_some());
    //             assert!(payload.param0.e.is_none());
    //             let array = payload.param0.d.as_ref().unwrap();
    //             assert_eq!(array.len(), 3);
    //             for u in array.iter() {
    //                 match *u {
    //                     UnionA::b(ref val) => assert_eq!(*val, 10),
    //                     _ => panic!("Incorrect union variant! Tag found: {}", u.get_tag()),
    //                 }
    //             }
    //         }
    //     }
    // }
    // conformance_mthd15_good_union_in_map {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 15);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod15Request => {
    //         |payload: &ConformanceTestInterfaceMethod15Request| {
    //             assert_eq!(payload.param0.a, true);
    //             assert_eq!(payload.param0.b, 22);
    //             assert!(payload.param0.c.is_none());
    //             assert!(payload.param0.d.is_none());
    //             assert!(payload.param0.e.is_some());
    //             let map = payload.param0.e.as_ref().unwrap();
    //             assert_eq!(map.len(), 3);
    //             let mut expect_keys = HashMap::with_capacity(3);
    //             expect_keys.insert(8, false);
    //             expect_keys.insert(7, false);
    //             expect_keys.insert(1, false);
    //             for (key, value) in map.iter() {
    //                 expect_keys.insert(*key, true);
    //                 match *value {
    //                     UnionA::b(ref val) => assert_eq!(*val, 10),
    //                     _ => panic!("Incorrect union variant! Tag found: {}", value.get_tag()),
    //                 }
    //             }
    //             for (key, value) in expect_keys.iter() {
    //                 if *value == false {
    //                     panic!("Expected key `{}`, but not found!", *key);
    //                 }
    //             }
    //         }
    //     }
    // }
    // conformance_mthd15_good_union_in_struct {
    //     MessageHeader => {
    //         |header: MessageHeader| {
    //             assert_eq!(header.version, 0);
    //             assert_eq!(header.name, 15);
    //             assert_eq!(header.flags, 0);
    //         }
    //     },
    //     ConformanceTestInterfaceMethod15Request => {
    //         |payload: &ConformanceTestInterfaceMethod15Request| {
    //             assert_eq!(payload.param0.a, true);
    //             assert_eq!(payload.param0.b, 22);
    //             assert!(payload.param0.c.is_some());
    //             assert!(payload.param0.d.is_none());
    //             assert!(payload.param0.e.is_none());
    //             let union_val = payload.param0.c.as_ref().unwrap();
    //             match *union_val {
    //                 UnionA::b(ref val) => assert_eq!(*val, 54),
    //                 _ => panic!("Incorrect union variant! Tag found: {}", union_val.get_tag()),
    //             }
    //         }
    //     }
    // }
}
