// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/tests/interop_tests:interop_test_mojom_rust";
    "//mojo/public/rust/mojom_value_parser:parser_unittests_rust";
    "//mojo/public/rust/system";
    "//mojo/public/rust/sequences";
    "//mojo/public/rust/sequences:test_cxx";
}

use bindings::remote::PendingRemote;
use interop_test_mojom_rust::interop_test::TestService;
use parser_unittests_rust::parser_unittests::*;
use sequences::run_loop::RunLoop;
use system::mojo_types::UntypedHandle;

#[cxx::bridge(namespace = "interop_test")]
pub mod ffi {
    unsafe extern "C++" {
        include!("mojo/public/rust/tests/interop_tests/interop_test.h");
        type TestServiceImpl;
        pub fn GetTestService(handle: &mut usize) -> UniquePtr<TestServiceImpl>;
    }
}

#[gtest(RustMojoInterop, InteropTest)]
fn test_interop() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let mut handle_val: usize = 0;
    let _impl = ffi::GetTestService(&mut handle_val);
    // SAFETY: The C++ side promises that this is a valid and unowned handle.
    let handle = unsafe { UntypedHandle::wrap_raw_value(handle_val) };
    let pending_remote = PendingRemote::<dyn TestService>::new(handle.into());
    let mut remote = pending_remote.bind();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    // 1. Basic Types
    let f = FourInts { a: 1, b: 2, c: 3, d: 4 };
    remote.PassFourInts(f.clone(), move |res| {
        expect_eq!(f, res);
    });

    // 2. Nested Types
    let t = TwiceNested {
        o: OnceNested {
            f1: FourInts { a: 1, b: 2, c: 3, d: 4 },
            a: 5,
            b: 6,
            f2: FourIntsReversed { d: 7, c: 8, b: 9, a: 10 },
            f3: FourIntsIntermixed { a: 11, b: 12, c: 13, d: 14 },
            c: 15,
        },
        a: 16,
        f: FourInts { a: 17, b: 18, c: 19, d: 20 },
        b: 21,
        c: 22,
    };
    remote.PassNested(t.clone(), move |res| {
        expect_eq!(t, res);
    });

    // 3. Bools
    let b = TenBoolsAndTwoBytes {
        b0: true,
        b1: false,
        b2: true,
        b3: false,
        b4: true,
        n1: 0x1234,
        b5: false,
        b6: true,
        b7: false,
        b8: true,
        b9: false,
    };
    remote.PassBools(b.clone(), move |res| {
        expect_eq!(b, res);
    });

    // 4. Enums
    let e = SomeEnums { e1: TestEnum::Three, n1: 12345678, e2: TestEnum2::FourtyTwo };
    remote.PassEnums(e.clone(), move |res| {
        expect_eq!(e, res);
    });

    // 5. Union
    let u = BaseUnion::f1(FourInts { a: 1, b: 2, c: 3, d: 4 });
    remote.PassUnion(u.clone(), move |res| {
        expect_eq!(u, res);
    });

    // 6. Nested Union
    let u_nested = WithManyUnions {
        u1: NestedUnion::u(BaseUnion::n1(5)),
        i1: 1,
        u2: NestederUnion::u(NestedUnion::n(10)),
        d1: 1.0,
        u3: BaseUnion::b1(true),
        u4: NestederUnion::b(false),
        i2: 2,
        i3: 3,
    };
    remote.PassNestedUnion(u_nested.clone(), move |res| {
        expect_eq!(u_nested, res);
    });

    // 7. Arrays
    let a = Arrays {
        ints: vec![1, 2, 3],
        ints_sized: [4, 5, 6],
        bools: vec![true, false, true],
        bool_sized: [
            true, false, true, false, true, false, true, false, true, false, true, false, true,
        ],
        floats: vec![1.0, 2.0],
        enums: vec![TestEnum::Zero, TestEnum::Seven],
        unions: vec![BaseUnion::n1(1), BaseUnion::b1(true)],
        unions_nested: vec![NestedUnion::n(10)],
        fourints: vec![FourInts { a: 1, b: 2, c: 3, d: 4 }],
        nested: vec![vec![1, 2], vec![3, 4]],
        nested_sized: [[1, 2], [3, 4], [5, 6]],
    };
    remote.PassArrays(a.clone(), move |res| {
        expect_eq!(a, res);
    });

    // 8. Maps
    let mut eights = std::collections::HashMap::new();
    eights.insert(1, 2);
    let mut bools = std::collections::HashMap::new();
    bools.insert(true, 100);
    let m = Maps {
        eights,
        bools,
        enums: Default::default(),
        to_struct: Default::default(),
        to_union: Default::default(),
        to_map: Default::default(),
        float_map: Default::default(),
    };
    remote.PassMaps(m.clone(), move |res| {
        expect_eq!(m, res);
    });

    // 9. Strings
    let s = Strings {
        str: "Hello".to_string(),
        arr: vec!["World".to_string()],
        to_str: Default::default(),
        from_str: Default::default(),
    };
    remote.PassStrings(s.clone(), move |res| {
        expect_eq!(s, res);
    });

    // 10. Nullables
    let n = NullableBasics {
        b: Some(true),
        n1: None,
        n2: Some(5),
        empty: None,
        e: Some(TestEnum::Four),
        fourints: Some(FourInts { a: 1, b: 2, c: 3, d: 4 }),
        f1: Some(1.0),
        f2: None,
    };
    remote.PassNullableBasics(n.clone(), move |res| {
        expect_eq!(n, res);
    });

    // 11. Nullable Others - This is the last call, so quit the run loop in the
    //     callback.
    let n_others = NullableOthers {
        u: Some(UnionWithNullables::str(Some("Hello".to_string()))),
        m: Some(Default::default()),
        str: Some("World".to_string()),
    };
    remote.PassNullableOthers(n_others.clone(), move |res| {
        expect_eq!(n_others, res);
        quit();
    });
}
