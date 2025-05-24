// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestURLSelectionOperation {
  async run(urls, data) {
    {{RUN_FUNCTION_BODY}}
    return 0;
  }
}

register('test-url-selection-operation', TestURLSelectionOperation);
