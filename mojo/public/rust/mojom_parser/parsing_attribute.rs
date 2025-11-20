// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

use quote::quote;
use syn::{parse_macro_input, DeriveInput};

#[proc_macro_derive(MojomParse)]
pub fn derive_mojomparse(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = input.ident;

    let struct_fields = match input.data {
        syn::Data::Struct(syn::DataStruct { fields, .. }) => match fields {
            syn::Fields::Named(syn::FieldsNamed { named, .. }) => named,
            _ => todo!("Gotta have named fields, give an error message"),
        },
        _ => todo!("Only structs supported at the moment"),
    };

    let num_fields = struct_fields.len();

    // As far as I know, quote can only iterate over vectors of things that can
    // be directly converted to tokens. Notably, this means they have to be
    // single values. So if we want to write something like #name = #val, in a
    // loop, we first have to combine each pair of names and values into a single
    // token stream, and then can we iterate over that in the quote.

    // The names of the fields in the struct.
    let field_idents: Vec<&syn::Ident> =
        struct_fields.iter().map(|field| field.ident.as_ref().unwrap()).collect();

    // A bunch of entries for a MojomType::Struct
    let mojom_type_fields: Vec<proc_macro2::TokenStream> = struct_fields
        .iter()
        .map(|field| {
            let ty = &field.ty;
            let name = field.ident.as_ref().unwrap().to_string();
            quote! { (#name.to_string(), #ty::mojom_type()) }
        })
        .collect();

    // A bunch of entries for a MojomValue::Struct
    let to_mojom_value_fields: Vec<proc_macro2::TokenStream> = struct_fields
        .iter()
        .map(|field| {
            let name = field.ident.as_ref().unwrap();
            let name_str = name.to_string();
            quote! { (#name_str.to_string(), value.#name.into()) }
        })
        .collect();

    // The body of a struct value, converting each field from a MojomValue with
    // the same name as the field.
    let from_mojom_value_fields: Vec<proc_macro2::TokenStream> = struct_fields
        .iter()
        .map(|field| {
            let name = field.ident.as_ref().unwrap();
            quote! { #name: #name.try_into()? }
        })
        .collect();

    // We wrap the `impl` blocks in an anonymous scope so that we can
    // import mojom_parser_core without polluting the caller's namespace.
    let quoted = quote! {
        const _: () = {
            chromium::import! {
                "//mojo/public/rust/mojom_parser:mojom_parser_core";
            }

            use mojom_parser_core::*;

            impl MojomParse for #name {
                fn mojom_type() -> MojomType {
                    let fields : Vec<(String, MojomType)> = vec![
                        #(#mojom_type_fields),*
                    ];
                    MojomType::Struct { fields }
                }
            }

            impl From<#name> for MojomValue {
                fn from(value: #name) -> MojomValue {
                    let fields : Vec<(String, MojomValue)> = vec![
                        #(#to_mojom_value_fields),*
                    ];
                    MojomValue::Struct ( fields )
                }
            }

            impl TryFrom<MojomValue> for #name {
                type Error = ::anyhow::Error;

                fn try_from(value : MojomValue) -> ::anyhow::Result<Self> {
                    use ::anyhow::Context;
                    // FOR_RELEASE: Don't clone here
                    if let MojomValue::Struct(fields) = value.clone() {
                        // Drop the strings, we don't care about them here
                        let fields : Vec<MojomValue> = fields.into_iter().map(|field| field.1).collect();
                        // Try to extract all the field values at once
                        let fields : [MojomValue; #num_fields] = fields.try_into()
                        .or(Err(::anyhow::anyhow!(
                                "Wrong number of fields to construct a value of type {} from MojomValue {:?}",
                                    std::any::type_name::<#name>(),
                                    value)))?;
                        let [#(#field_idents),*] = fields;
                        return Ok(Self {
                            #(#from_mojom_value_fields),*
                        })
                    } else {
                        ::anyhow::bail!(
                            "Cannot construct a value of type {} from non-struct MojomValue {:?}",
                            std::any::type_name::<#name>(),
                            value
                        );
                    }
                }
            }
        };
    };

    // Excellent for debugging, prints out the entire generated code
    // println!("{}", &quoted);
    return proc_macro::TokenStream::from(quoted);
}

#[proc_macro_derive(PrimitiveEnum)]
pub fn derive_primitiveenum(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = input.ident;

    let variants = match input.data {
        syn::Data::Enum(syn::DataEnum { variants, .. }) => variants,
        _ => panic!("No structs or unions allowed!"),
    };

    let mut next_discriminant: u32 = 0;
    let mut default_variant: Option<syn::Ident> = None;
    let generate_branch = |variant: syn::Variant| {
        if !variant.fields.is_empty() {
            panic!("Mojom enums must not have any variants with fields!")
        }

        // FOR_RELEASE: See if any variants have a "default" attribute
        default_variant = None; // Silence compiler until we do that

        let discriminant = match variant.discriminant {
            Some((_, syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(n), .. }))) => {
                let discriminant = n
                    .base10_parse::<u32>()
                    .expect("Enum discriminants must be a 32-bit integer literal.");
                discriminant
            }
            None => next_discriminant,
            _ => panic!("Enum discriminants must be a 32-bit integer literal."),
        };
        next_discriminant = discriminant + 1;

        let variant_name = variant.ident;

        quote! {
            #discriminant => Ok(#name::#variant_name)
        }
    };

    let mut branches = variants.into_iter().map(generate_branch).collect::<Vec<_>>();
    if let Some(default) = default_variant {
        branches.push(quote! { _ => Ok(#default)})
    } else {
        branches.push(quote! { _ => Err(anyhow::anyhow!(
            "Invalid discriminant {value} for type {}",
            std::any::type_name::<#name>()
        ))})
    }

    let quoted = quote! {
        const _ : () = {
            chromium::import! {
                "//mojo/public/rust/mojom_parser:mojom_parser_core";
            }

            use mojom_parser_core::*;

            impl From<#name> for u32 {
                fn from(value: #name) -> u32 { value as u32 }
            }

            impl TryFrom<u32> for #name {
                type Error = ::anyhow::Error;

                fn try_from(value : u32) -> ::anyhow::Result<Self> {
                    match value {
                        #(#branches),*
                    }
                }
            }

            impl TryFrom<MojomValue> for #name {
                type Error = ::anyhow::Error;

                fn try_from(value: MojomValue) -> ::anyhow::Result<Self> {
                    if let MojomValue::Enum(v) = value {
                        Ok(Self::try_from(v)?)
                    } else {
                        ::anyhow::bail!(
                            "Cannot construct a value of type {} from non-enum MojomValue {:?}",
                            std::any::type_name::<#name>(),
                            value
                        )
                    }
                }
            }

            impl PrimitiveEnum for #name {}
        };
    };

    return proc_macro::TokenStream::from(quoted);
}
