// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This build.rs generates some parameters for Cargo which are necessary to build.
//! Particularly, it searches for libsystem_thunks.a.

use std::env;
use std::ffi::OsStr;
use std::fs;
use std::path::Path;
use std::path::PathBuf;
use std::vec::Vec;

/// Searches a directory recursively for libsystem_thunks.a
/// and returns the path if found, or None if not found.
fn search_output(p: &Path, search_file: &OsStr) -> Option<PathBuf> {
    let mut dirs = Vec::new();
    let paths = fs::read_dir(p).unwrap();
    for dir_entry in paths {
        let path = dir_entry.unwrap().path();
        if path.is_dir() {
            dirs.push(path);
        } else {
            let found = match path.file_name() {
                Some(name) => name == search_file,
                None => false,
            };
            if found {
                return Some(p.to_path_buf());
            }
        }
    }
    for path in dirs.iter() {
        match search_output(path.as_path(), search_file) {
            Some(thunks_path) => return Some(thunks_path),
            None => continue,
        }
    }
    None
}

fn main() {
    let mojo_out_dir = Path::new(env!("MOJO_OUT_DIR"));
    let embed = match env::var("MOJO_RUST_NO_EMBED") {
        Ok(_) => false,
        Err(_) => true,
    };
    if mojo_out_dir.is_dir() {
        if !embed {
            match search_output(mojo_out_dir, OsStr::new("libsystem_thunks.a")) {
                Some(path) => {
                    println!("cargo:rustc-link-lib=static=system_thunks");
                    println!("cargo:rustc-link-search=native={}", path.display());
                }
                None => panic!("Failed to find system_thunks."),
            }
        } else {
            println!("cargo:rustc-link-lib=stdc++");
            match search_output(mojo_out_dir, OsStr::new("libvalidation_parser.a")) {
                Some(path) => {
                    println!("cargo:rustc-link-lib=static=validation_parser");
                    println!("cargo:rustc-link-search=native={}", path.display());
                }
                None => panic!("Failed to find validation_parser."),
            }
            match search_output(mojo_out_dir, OsStr::new("librust_embedder.a")) {
                Some(path) => {
                    println!("cargo:rustc-link-lib=static=rust_embedder");
                    println!("cargo:rustc-link-search=native={}", path.display());
                }
                None => panic!("Failed to find rust_embedder."),
            }
        }
    } else {
        panic!("MOJO_OUT_DIR is not a valid directory.");
    }
}
