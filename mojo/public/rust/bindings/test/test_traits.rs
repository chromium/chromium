// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use super::TypemappedStruct;

#[derive(Debug, PartialEq, Clone)]
pub struct MyCustomStruct {
    pub value: i32,
}

impl From<MyCustomStruct> for TypemappedStruct {
    fn from(custom: MyCustomStruct) -> Self {
        Self { a: custom.value }
    }
}

impl TryFrom<TypemappedStruct> for MyCustomStruct {
    type Error = anyhow::Error;
    fn try_from(mojo: TypemappedStruct) -> Result<Self, Self::Error> {
        Ok(Self { value: mojo.a })
    }
}
