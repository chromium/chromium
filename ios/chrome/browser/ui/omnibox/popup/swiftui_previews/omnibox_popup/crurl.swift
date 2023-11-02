// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// This class is an inexact copy of the Chrome CrURL class, necessary for the
/// `OmniboxIcon` protocol. The Chrome CrURL file can't be imported directly
/// because it requires more Chrome files, including generated header files
/// (after importing gurl.h), and this project doesn't use gn.
/// TODO(crbug.com/1303895): Remove this class when it's no longer necessary.
public class CrURL: NSObject {
  private(set) var url: URL

  init(url: URL) {
    self.url = url
  }
}
