// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::TokenStream;
use quote::{format_ident, quote, quote_spanned, ToTokens};
use syn::parse::{Parse, ParseStream};
use syn::spanned::Spanned;
use syn::{parse_macro_input, Error, Ident, ItemFn, ItemImpl, LitStr, Token, Type};

/// The prefix attached to a Gtest factory function by the
/// RUST_GTEST_TEST_SUITE_FACTORY() macro.
const RUST_GTEST_FACTORY_PREFIX: &str = "RustGtestFactory_";

struct GtestArgs {
    suite_name: String,
    test_name: String,
}

impl Parse for GtestArgs {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let suite_name = input.parse::<Ident>()?.to_string();
        input.parse::<Token![,]>()?;
        let test_name = input.parse::<Ident>()?.to_string();
        Ok(GtestArgs { suite_name, test_name })
    }
}

struct GtestSuiteArgs {
    rust_type: Type,
}

impl Parse for GtestSuiteArgs {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let rust_type = input.parse::<Type>()?;
        Ok(GtestSuiteArgs { rust_type })
    }
}

struct ExternTestSuiteArgs {
    cpp_type: TokenStream,
}

impl Parse for ExternTestSuiteArgs {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        // TODO(b/229791967): With CXX it is not possible to get the C++ typename and
        // path from the Rust wrapper type, so we require specifying it by hand in
        // the macro. It would be nice to remove this opportunity for mistakes.
        let cpp_type_as_lit_str = input.parse::<LitStr>()?;

        // TODO(danakj): This code drops the C++ namespaces, because we can't produce a
        // mangled name and can't generate bindings involving fn pointers, so we require
        // the C++ function to be `extern "C"` which means it has no namespace.
        // Eventually we should drop the `extern "C"` on the C++ side and use the
        // full path here.
        match cpp_type_as_lit_str.value().split("::").last() {
            Some(name) => {
                Ok(ExternTestSuiteArgs { cpp_type: format_ident!("{}", name).into_token_stream() })
            }
            None => Err(Error::new(cpp_type_as_lit_str.span(), "invalid C++ class name")),
        }
    }
}

struct CppPrefixArgs {
    cpp_prefix: String,
}

impl Parse for CppPrefixArgs {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let cpp_prefix_as_lit_str = input.parse::<LitStr>()?;
        Ok(CppPrefixArgs { cpp_prefix: cpp_prefix_as_lit_str.value() })
    }
}

/// The `gtest` macro can be placed on a function to make it into a Gtest unit
/// test, when linked into a C++ binary that invokes Gtest.
///
/// The `gtest` macro takes two arguments, which are Rust identifiers. The first
/// is the name of the test suite and the second is the name of the test, each
/// of which are converted to a string and given to Gtest. The name of the test
/// function itself does not matter, and need not be unique (it's placed into a
/// unique module based on the Gtest suite + test names.
///
/// The test function must have no arguments. The return value must be either
/// `()` or `std::result::Result<(), E>`. If another return type is found, the
/// test will fail when run. If the return type is a `Result`, then an `Err` is
/// treated as a test failure.
///
/// # Examples
/// ```
/// #[gtest(MathTest, Addition)]
/// fn my_test() {
///   expect_eq!(1 + 1, 2);
/// }
/// ```
///
/// The above adds the function to the Gtest binary as `MathTest.Addtition`:
/// ```
/// [ RUN      ] MathTest.Addition
/// [       OK ] MathTest.Addition (0 ms)
/// ```
///
/// A test with a Result return type, and which uses the `?` operator. It will
/// fail if the test returns an `Err`, and print the resulting error string:
/// ```
/// #[gtest(ResultTest, CheckThingWithResult)]
/// fn my_test() -> std::result::Result<(), String> {
///   call_thing_with_result()?;
/// }
/// ```
#[proc_macro_attribute]
pub fn gtest(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let GtestArgs { suite_name, test_name } = parse_macro_input!(args as GtestArgs);

    let (input_fn, gtest_suite_attr) = {
        let mut input_fn = parse_macro_input!(input as ItemFn);

        if let Some(asyncness) = input_fn.sig.asyncness {
            // TODO(crbug.com/40211749): We can support async functions once we have
            // block_on() support which will run a RunLoop until the async test
            // completes. The run_test_fn just needs to be generated to `block_on(||
            // #test_fn)` instead of calling `#test_fn` synchronously.
            return quote_spanned! {
                asyncness.span =>
                    compile_error!("async functions are not supported.");
            }
            .into();
        }

        // Filter out other gtest attributes on the test function and save them for
        // later processing.
        let mut gtest_suite_attr = None;
        input_fn.attrs = input_fn
            .attrs
            .into_iter()
            .filter_map(|attr| {
                if attr.path().is_ident("gtest_suite") {
                    gtest_suite_attr = Some(attr);
                    None
                } else {
                    Some(attr)
                }
            })
            .collect::<Vec<_>>();

        (input_fn, gtest_suite_attr)
    };

    // The identifier of the function which contains the body of the test.
    let test_fn = &input_fn.sig.ident;

    let (gtest_factory_fn, test_fn_call) = if let Some(attr) = gtest_suite_attr {
        // If present, the gtest_suite attribute is expected to have the form
        // `#[gtest_suite(path::to::RustType)]`. The Rust type wraps a C++
        // `TestSuite` (subclass of `::testing::Test`) which should be created
        // and returned by a C++ factory function.
        let rust_type = match attr.parse_args::<GtestSuiteArgs>() {
            Ok(x) => x.rust_type,
            Err(x) => return x.to_compile_error().into(),
        };

        (
            // Get the Gtest factory function pointer from the TestSuite trait.
            quote! { <#rust_type as ::rust_gtest_interop::TestSuite>::gtest_factory_fn_ptr() },
            // SAFETY: Our lambda casts the `suite` reference and does not move from it, and
            // the resulting type is not Unpin.
            quote! {
                let p = unsafe {
                    suite.map_unchecked_mut(|suite: &mut ::rust_gtest_interop::OpaqueTestingTest| {
                        suite.as_mut()
                    })
                };
                #test_fn(p)
            },
        )
    } else {
        // Otherwise, use `rust_gtest_interop::rust_gtest_default_factory()`
        // which makes a `TestSuite` with `testing::Test` directly.
        (
            quote! { ::rust_gtest_interop::__private::rust_gtest_default_factory },
            quote! { #test_fn() },
        )
    };

    // The test function and all code generate by this proc macroa go into a
    // submodule which is uniquely named for the super module based on the Gtest
    // suite and test names. If two tests have the same suite + test name, this
    // will result in a compiler error—this is OK because Gtest disallows
    // dynamically registering multiple tests with the same suite + test name.
    let test_mod = format_ident!("__test_{}_{}", suite_name, test_name);

    // In the generated code, `run_test_fn` is marked #[no_mangle] to work around a
    // codegen bug where the function is seen as dead and the compiler omits it
    // from the object files. Since it's #[no_mangle], the identifier must be
    // globally unique or we have an ODR violation. To produce a unique
    // identifier, we roll our own name mangling by combining the file name and
    // path from the source tree root with the Gtest suite and test names and the
    // function itself.
    //
    // Note that an adversary could still produce a bug here by placing two equal
    // Gtest suite and names in a single .rs file but in separate inline
    // submodules.
    //
    // TODO(dcheng): This probably can be simplified to not bother with anything
    // other than the suite and test name, given Gtest's restrictions for a
    // given suite + test name pair to be globally unique within a test binary.
    let mangled_function_name = |f: &syn::ItemFn| -> syn::Ident {
        let file_name = file!().replace(|c: char| !c.is_ascii_alphanumeric(), "_");
        format_ident!("{}_{}_{}_{}", file_name, suite_name, test_name, f.sig.ident)
    };

    let run_test_fn = format_ident!("run_test_{}", mangled_function_name(&input_fn));

    // Implements ToTokens to generate a reference to a static-lifetime,
    // null-terminated, C-String literal. It is represented as an array of type
    // std::os::raw::c_char which can be either signed or unsigned depending on
    // the platform, and it can be passed directly to C++. This differs from
    // byte strings and CStr which work with `u8`.
    //
    // TODO(crbug.com/40215436): Would it make sense to write a c_str_literal!()
    // macro that takes a Rust string literal and produces a null-terminated
    // array of `c_char`? Then you could write `c_str_literal!(file!())` for
    // example, or implement a `file_c_str!()` in this way. Explore using https://crates.io/crates/cstr.
    //
    // TODO(danakj): Write unit tests for this, and consider pulling this out into
    // its own crate, if we don't replace it with c_str_literal!() or the "cstr"
    // crate.
    struct CStringLiteral<'a>(&'a str);
    impl quote::ToTokens for CStringLiteral<'_> {
        fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
            let mut c_chars = self.0.chars().map(|c| c as std::os::raw::c_char).collect::<Vec<_>>();
            c_chars.push(0);
            // Verify there's no embedded nulls as that would be invalid if the literal were
            // put in a std::ffi::CString.
            assert_eq!(c_chars.iter().filter(|x| **x == 0).count(), 1);
            let comment = format!("\"{}\" as [c_char]", self.0);
            tokens.extend(quote! {
                {
                    #[doc=#comment]
                    &[#(#c_chars as std::os::raw::c_char),*]
                }
            });
        }
    }

    // C-compatible string literals, that can be inserted into the quote! macro.
    let suite_name_c_bytes = CStringLiteral(&suite_name);
    let test_name_c_bytes = CStringLiteral(&test_name);
    let file_c_bytes = CStringLiteral(file!());

    let output = quote! {
        #[cfg(not(is_gtest_unittests))]
        compile_error!(
            "#[gtest(...)] can only be used in targets where the GN \
            variable `is_gtest_unittests` is set to `true`.");

        mod #test_mod {
            use super::*;

            #[::rust_gtest_interop::small_ctor::ctor]
            unsafe fn register_test() {
                let r = ::rust_gtest_interop::__private::TestRegistration {
                    func: #run_test_fn,
                    test_suite_name: #suite_name_c_bytes,
                    test_name: #test_name_c_bytes,
                    file: #file_c_bytes,
                    line: line!(),
                    factory: #gtest_factory_fn,
                };
                ::rust_gtest_interop::__private::register_test(r);
            }

            // The function is extern "C" so `register_test()` can pass this fn as a pointer to C++
            // where it's registered with gtest.
            //
            // TODO(crbug.com/40214720): Removing #[no_mangle] makes rustc drop the symbol for the
            // test function in the generated rlib which produces linker errors. If we resolve the
            // linked bug and emit real object files from rustc for linking, then all the required
            // symbols are present and `#[no_mangle]` should go away along with the custom-mangling
            // of `run_test_fn`. We can not use `pub` to resolve this unfortunately. When `#[used]`
            // is fixed in https://github.com/rust-lang/rust/issues/47384, this may also be
            // resolved as well.
            #[no_mangle]
            extern "C" fn #run_test_fn(
                suite: std::pin::Pin<&mut ::rust_gtest_interop::OpaqueTestingTest>
            ) {
                let catch_result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                    #test_fn_call
                }));
                use ::rust_gtest_interop::TestResult;
                let err_message: Option<String> = match catch_result {
                    Ok(fn_result) => TestResult::into_error_message(fn_result),
                    Err(_) => Some("Test panicked".to_string()),
                };
                if let Some(m) = err_message.as_ref() {
                    ::rust_gtest_interop::__private::add_failure_at(file!(), line!(), &m);
                }
            }

            #input_fn
        }
    };

    output.into()
}

/// The `#[extern_test_suite()]` macro is used to implement the unsafe
/// `TestSuite` trait.
///
/// The `TestSuite` trait is used to mark a Rust type as being a wrapper of a
/// C++ subclass of `testing::Test`. This makes it valid to cast from a `*mut
/// testing::Test` to a pointer of the marked Rust type.
///
/// It also marks a promise that on the C++, there exists an instantiation of
/// the RUST_GTEST_TEST_SUITE_FACTORY() macro for the C++ subclass type which
/// will be linked with the Rust crate.
///
/// The macro takes a single parameter which is the fully specified C++ typename
/// of the C++ subclass for which the implementing Rust type is a wrapper. It
/// expects the body of the trait implementation to be empty, as it will fill in
/// the required implementation.
///
/// # Example
/// If in C++ we have:
/// ```cpp
/// class GoatTestSuite : public testing::Test {}
/// RUST_GTEST_TEST_SUITE_FACTORY(GoatTestSuite);
/// ```
///
/// And in Rust we have a `ffi::GoatTestSuite` type generated to wrap the C++
/// type. The the type can be marked as a valid TestSuite with the
/// `#[extern_test_suite]` macro: ```rs
/// #[extern_test_suite("GoatTestSuite")]
/// unsafe impl rust_gtest_interop::TestSuite for ffi::GoatTestSuite {}
/// ```
/// 
/// # Internals
/// The #[cpp_prefix("STRING_")] attribute can follow `#[extern_test_suite()]`
/// to control the path to the C++ Gtest factory function. This is used for
/// connecting to different C++ macros than the usual
/// RUST_GTEST_TEST_SUITE_FACTORY().
#[proc_macro_attribute]
pub fn extern_test_suite(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    // TODO(b/229791967): With CXX it is not possible to get the C++ typename and
    // path from the Rust wrapper type, so we require specifying it by hand in
    // the macro. It would be nice to remove this opportunity for mistakes.
    let ExternTestSuiteArgs { cpp_type } = parse_macro_input!(args as ExternTestSuiteArgs);

    // Filter out other gtest attributes on the trait impl and save them for later
    // processing.
    let (trait_impl, cpp_prefix_attr) = {
        let mut trait_impl = parse_macro_input!(input as ItemImpl);

        if !trait_impl.items.is_empty() {
            return quote_spanned! {trait_impl.items[0].span() => compile_error!(
                "expected empty trait impl"
            )}
            .into();
        }

        let mut cpp_prefix_attr = None;
        trait_impl.attrs = trait_impl
            .attrs
            .into_iter()
            .filter_map(|attr| {
                if attr.path().is_ident("cpp_prefix") {
                    cpp_prefix_attr = Some(attr);
                    None
                } else {
                    Some(attr)
                }
            })
            .collect::<Vec<_>>();

        (trait_impl, cpp_prefix_attr)
    };

    let cpp_prefix = if let Some(attr) = cpp_prefix_attr {
        // If present, the cpp_prefix attribute is expected to have the form
        // `#[cpp_prefix("PREFIX_STRING_")]`.
        match attr.parse_args::<CppPrefixArgs>() {
            Ok(cpp_prefix_args) => cpp_prefix_args.cpp_prefix,
            Err(x) => return x.to_compile_error().into(),
        }
    } else {
        RUST_GTEST_FACTORY_PREFIX.to_string()
    };

    let trait_name = match &trait_impl.trait_ {
        Some((_, path, _)) => path,
        None => {
            return quote! {compile_error!(
                "expected impl rust_gtest_interop::TestSuite trait"
            )}
            .into();
        }
    };

    let rust_type = match &*trait_impl.self_ty {
        Type::Path(type_path) => type_path,
        _ => {
            return quote_spanned! {trait_impl.self_ty.span() => compile_error!(
                "expected type that wraps C++ subclass of `testing::Test`"
            )}
            .into();
        }
    };

    // TODO(danakj): We should generate a C++ mangled name here, then we don't
    // require the function to be `extern "C"` (or have the author write the
    // mangled name themselves).
    let cpp_fn_name = format_ident!("{}{}", cpp_prefix, cpp_type.to_string());

    let output = quote! {
        unsafe impl #trait_name for #rust_type {
            fn gtest_factory_fn_ptr() -> rust_gtest_interop::GtestFactoryFunction {
                extern "C" {
                    fn #cpp_fn_name(
                        f: extern "C" fn(
                            test_body: ::std::pin::Pin<&mut ::rust_gtest_interop::OpaqueTestingTest>
                        )
                    ) -> ::std::pin::Pin<&'static mut ::rust_gtest_interop::OpaqueTestingTest>;
                }
                #cpp_fn_name
            }
        }
    };

    output.into()
}
