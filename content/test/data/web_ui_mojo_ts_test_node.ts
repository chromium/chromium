// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is effectively the same structure as NestedMappedType mojom,
// except it is implemented purely in ts and has a different field name
// to exercise the conversion logic.
//
// Using a class instead of an interface in order to have a constructor to
// instantiate objects of this type with "new"".
export class TestNode {
  next: TestNode|null;

  constructor(next?: TestNode|null) {
    this.next = next || null;
  }
}
