// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INIT_H_
#define PDF_PDF_INIT_H_

namespace chrome_pdf {

bool IsSDKInitializedViaPlugin();
void SetIsSDKInitializedViaPlugin(bool initialized_via_plugin);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INIT_H_
