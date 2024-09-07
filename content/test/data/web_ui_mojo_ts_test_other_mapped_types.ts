// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface MappedDictType {
  get(key: string): string|undefined;
  set(key: string, value: string): void;
  keys(): Iterable<string>;
}
