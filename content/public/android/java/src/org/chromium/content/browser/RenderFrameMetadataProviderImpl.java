// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("content::android")
class RenderFrameMetadataProviderImpl {
    private RenderFrameMetadataProviderImpl() {}

    private static class RenderFrameMetadataProviderRecursiveDeleteException
            extends RuntimeException {}

    @CalledByNative
    private static void reportRecursiveDelete() {
        JavaExceptionReporter.reportException(
                new RenderFrameMetadataProviderRecursiveDeleteException());
    }
}
