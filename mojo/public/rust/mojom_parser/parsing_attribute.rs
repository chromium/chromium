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

    let derived_tokens = match input.data {
        syn::Data::Struct(syn::DataStruct { fields, .. }) => match fields {
            syn::Fields::Named(syn::FieldsNamed { named, .. }) => {
                derive_mojomparse_struct(name, named)
            }
            _ => panic!("Mojom structs do not support unnamed fields"),
        },
        syn::Data::Enum(syn::DataEnum { variants, .. }) => derive_mojomparse_union(name, variants),
        syn::Data::Union(_) => {
            panic!("Mojom does not support untagged unions. Use a Rust enum instead.")
        }
    };

    return proc_macro::TokenStream::from(derived_tokens);
}

fn derive_mojomparse_struct(
    name: syn::Ident,
    struct_fields: syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
) -> proc_macro2::TokenStream {
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
            let name = &field.ident;
            let name_str = field.ident.as_ref().unwrap().to_string();
            quote! { (#name_str.to_string(), value.#name.into()) }
        })
        .collect();

    // The body of a struct value, converting each field from a MojomValue with
    // the same name as the field.
    let from_mojom_value_fields: Vec<proc_macro2::TokenStream> = struct_fields
        .iter()
        .map(|field| {
            let name = &field.ident;
            quote! { #name: #name.try_into()? }
        })
        .collect();

    // We wrap the `impl` blocks in an anonymous scope so that we can
    // import mojom_parser_core without polluting the caller's namespace.
    return quote! {
        const _: () = {
            chromium::import! {
                "//mojo/public/rust/mojom_parser:mojom_parser_core";
            }

            use mojom_parser_core::*;

            impl MojomParse for #name {
                fn mojom_type() -> MojomType {
                    let (field_names, fields) : (Vec<String>, Vec<MojomType>) = vec![
                        #(#mojom_type_fields),*
                    ]
                    .into_iter().unzip();
                    MojomType::Struct { field_names, fields }
                }
            }

            impl From<#name> for MojomValue {
                fn from(value: #name) -> MojomValue {
                    let (field_names, fields) : (Vec<String>, Vec<MojomValue>) = vec![
                        #(#to_mojom_value_fields),*
                    ]
                    .into_iter().unzip();
                    MojomValue::Struct ( field_names, fields )
                }
            }

            impl TryFrom<MojomValue> for #name {
                type Error = ::anyhow::Error;

                fn try_from(value : MojomValue) -> ::anyhow::Result<Self> {
                    // FOR_RELEASE: Don't clone here
                    if let MojomValue::Struct(_field_names, fields) = value.clone() {
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
}

fn derive_mojomparse_union(
    name: syn::Ident,
    variants: syn::punctuated::Punctuated<syn::Variant, syn::Token![,]>,
) -> proc_macro2::TokenStream {
    // Extract/compute just the bits of the variants that we care about:
    // The name, type, and discriminant value
    let mut next_discriminant: u32 = 0;
    let variant_info : Vec<(syn::Ident, syn::Type, u32)> = variants.into_iter().map(|variant: syn::Variant| {
        let variant_name = variant.ident;
        let mut fields = match variant.fields {
            syn::Fields::Unnamed(syn::FieldsUnnamed { unnamed, .. }) => unnamed,
            syn::Fields::Named(syn::FieldsNamed { named, .. }) => named,
            syn::Fields::Unit => syn::punctuated::Punctuated::new(), // We'll panic shortly
        };
        if fields.len() != 1 {
            let mut panic_msg = format!("Variant {variant_name} of enum {name} must have exactly one field to be serialized as a Mojom union.");
            if fields.len() == 0 {
                panic_msg.push_str("\nTo serialize as a Mojom enum, derive PrimitiveEnum instead, which will automatically provide MojomParse.")
            }
            panic!("{}", panic_msg)
        }
        let field_ty = fields.pop().unwrap().into_value().ty;
        let discriminant = compute_next_discriminant(&mut next_discriminant, variant.discriminant);
        (variant_name, field_ty, discriminant)
    }).collect();

    let mojom_type_fields = variant_info
        .iter()
        .map(|(_, ty, discriminant)| quote! { (#discriminant, #ty::mojom_type()) });
    let to_mojom_value_branches = variant_info
        .iter()
        .map(|(variant_name, _, discriminant)| quote! { #name::#variant_name(v) => (#discriminant, v.into()) });
    let from_mojom_value_branches = variant_info.iter().map(|(name, _, discriminant)| {
        // boxed_value is defined by the surrounding scope
        quote! { #discriminant => Ok(Self::#name((*boxed_value).try_into()?)), }
    });

    return quote! {
        const _: () = {
            chromium::import! {
                "//mojo/public/rust/mojom_parser:mojom_parser_core";
            }

            use mojom_parser_core::*;
            use std::collections::HashMap;

            impl MojomParse for #name {
                fn mojom_type() -> MojomType {
                    let variants : HashMap<u32, MojomType> = [
                        #(#mojom_type_fields),*
                    ].into();
                    MojomType::Union { variants }
                }
            }

            impl From<#name> for MojomValue {
                fn from(value: #name) -> MojomValue {
                    let (discriminant, mojom_value) = match value {
                        #(#to_mojom_value_branches),*
                    };
                    MojomValue::Union ( discriminant, Box::new(mojom_value) )
                }
            }

            impl TryFrom<MojomValue> for #name {
                type Error = ::anyhow::Error;

                fn try_from(value : MojomValue) -> ::anyhow::Result<Self> {
                    // FOR_RELEASE: Don't clone here
                    if let MojomValue::Union(discriminant, boxed_value) = value.clone() {
                        match discriminant {
                            #(#from_mojom_value_branches)*
                            _ => ::anyhow::bail!(
                                     "Invalid discriminant to construct a value of type {} from MojomValue {:?}",
                                     std::any::type_name::<#name>(),
                                     value)
                        }
                    } else {
                        ::anyhow::bail!(
                            "Cannot construct a value of type {} from non-union MojomValue {:?}",
                            std::any::type_name::<#name>(),
                            value
                        );
                    }
                }
            }
        };
    };
}

// Compute the discriminant for this field, as either the provided value or the
// value of `next_discriminant`. In either case, set `next_discriminant` to the
// computed value + 1
fn compute_next_discriminant(
    next_discriminant: &mut u32,
    discriminant_opt: Option<(syn::Token![=], syn::Expr)>,
) -> u32 {
    let discriminant = match discriminant_opt {
        Some((_, syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(n), .. }))) => {
            let discriminant = n
                .base10_parse::<u32>()
                .expect("Enum/Union discriminants must be a 32-bit integer literal.");
            discriminant
        }
        None => *next_discriminant,
        _ => panic!("Enum/Union discriminants must be a 32-bit integer literal."),
    };
    *next_discriminant = discriminant + 1;
    discriminant
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

        let discriminant = compute_next_discriminant(&mut next_discriminant, variant.discriminant);
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
