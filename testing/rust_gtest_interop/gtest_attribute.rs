use proc_macro2::{Span, TokenStream};
use quote::{format_ident, quote, quote_spanned, ToTokens};
use syn::spanned::Spanned;

// The C++ mangled name for rust_gtest_interop::rust_gtest_default_factory(). This comes from
// `objdump -t` on the C++ object file.
//
// TODO(danakj): We do this by hand because cxx doesn't support function pointers
// (https://github.com/dtolnay/cxx/issues/1011). We could wrap the function pointer in a struct,
// but then we have to pass it in UniquePtr a function pointer with 'static lifetime. We do this
// instead of introducing multiple levels of extra abstractions (a struct, unique_ptr) and
// leaking heap memory.
const RUST_GTEST_DEFAULT_FACTORY_CPP_MANGLED: &str =
    "_ZN18rust_gtest_interop26rust_gtest_default_factoryEPFvvE";

/// The `gtest` macro can be placed on a function to make it into a Gtest unit test, when linked
/// into a C++ binary that invokes Gtest.
///
/// The `gtest` macro takes two arguments, which are Rust identifiers. The first is the name of the
/// test suite and the second is the name of the test, each of which are converted to a string and
/// given to Gtest. The name of the test function itself does not matter, and need not be unique
/// (it's placed into a unique module based on the Gtest suite + test names.
///
/// The test function must have no arguments. The return value must be either `()` or
/// `std::result::Result<(), E>`. If another return type is found, the test will fail when run. If
/// the return type is a `Result`, then an `Err` is treated as a test failure.
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
/// A test with a Result return type, and which uses the `?` operator. It will fail if the test
/// returns an `Err`, and print the resulting error string:
/// ```
/// #[gtest(ResultTest, CheckThingWithResult)]
/// fn my_test() -> std::result::Result<(), String> {
///   call_thing_with_result()?;
/// }
/// ```
#[proc_macro_attribute]
pub fn gtest(
    arg_stream: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    enum GtestAttributeArgument {
        TestSuite,
        TestName,
    }
    // Returns a string representation of an identifier argument to the attribute. For example, for
    // #[gtest(Foo, Bar)], this function would return "Foo" for position 0 and "Bar" for position 1.
    // If the argument is not a Rust identifier or not present, it returns a compiler error as a
    // TokenStream to be emitted.
    fn get_arg_string(
        args: &syn::AttributeArgs,
        which: GtestAttributeArgument,
    ) -> Result<String, TokenStream> {
        let pos = match which {
            GtestAttributeArgument::TestSuite => 0,
            GtestAttributeArgument::TestName => 1,
        };
        match &args[pos] {
            syn::NestedMeta::Meta(syn::Meta::Path(path)) if path.segments.len() == 1 => {
                Ok(path.segments[0].ident.to_string())
            }
            _ => {
                let error_stream = match which {
                    GtestAttributeArgument::TestSuite => {
                        quote_spanned! {
                                args[pos].span() =>
                            compile_error!(
                                "Expected a test suite name, written as an identifier."
                            );
                        }
                    }
                    GtestAttributeArgument::TestName => {
                        quote_spanned! {
                                args[pos].span() =>
                            compile_error!(
                                "Expected a test name, written as an identifier."
                            );
                        }
                    }
                };
                Err(error_stream)
            }
        }
    }

    /// Parses `#[gtest_suite(path::to::function)]` and returns `path::to::function`.
    ///
    /// TODO(danakj): In the future could we replace this macro with
    /// `#[gtest_suite(path::to::CppClass)]` and inject a call to a templated function instead? We
    /// would need the definition of the CppClass available however.
    fn parse_gtest_suite(attr: &syn::Attribute) -> Result<TokenStream, TokenStream> {
        let parsed = match attr.parse_meta() {
            Ok(syn::Meta::List(list)) if list.nested.len() == 1 => match &list.nested[0] {
                syn::NestedMeta::Meta(syn::Meta::Path(fn_path)) => Ok(fn_path.into_token_stream()),
                x => Err(x.span()),
            },
            Ok(x) => Err(x.span()),
            Err(x) => Err(x.span()),
        };
        parsed.or_else(|span| {
            Err(quote_spanned! { span =>
                compile_error!(
                    "invalid syntax for gtest_suite macro, \
                    expected `#[gtest_suite(path::to::cpp_factory_fn)]`");
            })
        })
    }

    let args = syn::parse_macro_input!(arg_stream as syn::AttributeArgs);
    let mut input_fn = syn::parse_macro_input!(input as syn::ItemFn);

    let default_factory_fn =
        syn::Ident::new(RUST_GTEST_DEFAULT_FACTORY_CPP_MANGLED, Span::call_site());

    // The name of a C++ function which acts as the Gtest factory.
    let mut gtest_factory_fn = quote! {#default_factory_fn};

    // Look through other attributes on the test function, parse the ones related to Gtests, and put
    // the rest back into `attrs`.
    input_fn.attrs = {
        let mut keep = Vec::new();
        for attr in std::mem::take(&mut input_fn.attrs) {
            if attr.path.is_ident("gtest_suite") {
                let cpp_fn_name = match parse_gtest_suite(&attr) {
                    Ok(tokens) => tokens,
                    Err(error_tokens) => return error_tokens.into(),
                };
                // TODO(danakj): We should generate a C++ mangled name here, then we don't require
                // the function to be `extern "C"` (or have the author write the mangled name
                // themselves).
                gtest_factory_fn = quote! {#cpp_fn_name};
            } else {
                keep.push(attr)
            }
        }
        keep
    };

    // No longer mut.
    let input_fn = input_fn;
    let gtest_factory_fn = gtest_factory_fn;

    if let Some(asyncness) = input_fn.sig.asyncness {
        // TODO(crbug.com/1288947): We can support async functions once we have block_on() support
        // which will run a RunLoop until the async test completes. The run_test_fn just needs to be
        // generated to `block_on(|| #test_fn)` instead of calling `#test_fn` synchronously.
        return quote_spanned! {
            asyncness.span =>
            compile_error!("async functions are not supported.");
        }
        .into();
    }

    let (test_suite_name, test_name) = match args.len() {
        2 => {
            let suite = match get_arg_string(&args, GtestAttributeArgument::TestSuite) {
                Ok(ok) => ok,
                Err(error_stream) => return error_stream.into(),
            };
            let test = match get_arg_string(&args, GtestAttributeArgument::TestName) {
                Ok(ok) => ok,
                Err(error_stream) => return error_stream.into(),
            };
            (suite, test)
        }
        0 | 1 => {
            return quote! {
                compile_error!(
                    "Expected two arguments. For example: #[gtest(TestSuite, TestName)].");
            }
            .into();
        }
        x => {
            return quote_spanned! {
                args[x.min(2)].span() =>
                compile_error!(
                    "Expected two arguments. For example: #[gtest(TestSuite, TestName)].");
            }
            .into();
        }
    };

    // We put the test function and all the code we generate around it into a submodule which is
    // uniquely named for the super module based on the Gtest suite and test names. A result of this
    // is that if two tests have the same test suite + name, a compiler error would report the
    // conflict.
    let test_mod = format_ident!("__test_{}_{}", test_suite_name, test_name);

    // The run_test_fn identifier is marked #[no_mangle] to work around a codegen bug where the
    // function is seen as dead and the compiler omits it from the object files. Since it's
    // #[no_mangle], the identifier must be globally unique or we have an ODR violation. To produce
    // a unique identifier, we roll our own name mangling by combining the file name and path from
    // the source tree root with the Gtest suite and test names and the function itself.
    //
    // Note that an adversary could still produce a bug here by placing two equal Gtest suite and
    // names in a single .rs file but in separate inline submodules.
    //
    // TODO(danakj): Build a repro and file upstream bug to refer to.
    let mangled_function_name = |f: &syn::ItemFn| -> syn::Ident {
        let file_name = file!().replace(|c: char| !c.is_ascii_alphanumeric(), "_");
        format_ident!("{}_{}_{}_{}", file_name, test_suite_name, test_name, f.sig.ident)
    };
    let run_test_fn = format_ident!("run_test_{}", mangled_function_name(&input_fn));

    // The identifier of the function which contains the body of the test.
    let test_fn = &input_fn.sig.ident;

    // Implements ToTokens to generate a reference to a static-lifetime, null-terminated, C-String
    // literal. It is represented as an array of type std::os::raw::c_char which can be either
    // signed or unsigned depending on the platform, and it can be passed directly to C++. This
    // differs from byte strings and CStr which work with `u8`.
    //
    // TODO(crbug.com/1298175): Would it make sense to write a c_str_literal!() macro that takes a
    // Rust string literal and produces a null-terminated array of `c_char`? Then you could write
    // `c_str_literal!(file!())` for example, or implement a `file_c_str!()` in this way. Explore
    // using https://crates.io/crates/cstr.
    //
    // TODO(danakj): Write unit tests for this, and consider pulling this out into its own crate,
    // if we don't replace it with c_str_literal!() or the "cstr" crate.
    struct CStringLiteral<'a>(&'a str);
    impl quote::ToTokens for CStringLiteral<'_> {
        fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
            let mut c_chars = self.0.chars().map(|c| c as std::os::raw::c_char).collect::<Vec<_>>();
            c_chars.push(0);
            // Verify there's no embedded nulls as that would be invalid if the literal were put in
            // a std::ffi::CString.
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
    let test_suite_name_c_bytes = CStringLiteral(&test_suite_name);
    let test_name_c_bytes = CStringLiteral(&test_name);
    let file_c_bytes = CStringLiteral(file!());

    let gtest_factory_fn_signature = quote! {
        fn #gtest_factory_fn(f: extern "C" fn()) -> ::rust_gtest_interop::GtestSuitePtr;
    };

    let output = quote! {
        mod #test_mod {
            use super::*;
            use std::error::Error;
            use std::fmt::Display;
            use std::result::Result;

            extern "C" {
                #gtest_factory_fn_signature
            }

            #[::rust_gtest_interop::small_ctor::ctor]
            unsafe fn register_test() {
                let r = ::rust_gtest_interop::__private::TestRegistration {
                    func: #run_test_fn,
                    test_suite_name: #test_suite_name_c_bytes,
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
            // TODO(crbug.com/1296284): Removing #[no_mangle] makes rustc drop the symbol for the
            // test function in the generated rlib which produces linker errors. If we resolve the
            // linked bug and emit real object files from rustc for linking, then all the required
            // symbols are present and `#[no_mangle]` should go away along with the custom-mangling
            // of `run_test_fn`. We can not use `pub` to resolve this unfortunately. When `#[used]`
            // is fixed in https://github.com/rust-lang/rust/issues/47384, this may also be
            // resolved as well.
            #[no_mangle]
            extern "C" fn #run_test_fn() {
                let catch_result = std::panic::catch_unwind(|| #test_fn());
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
