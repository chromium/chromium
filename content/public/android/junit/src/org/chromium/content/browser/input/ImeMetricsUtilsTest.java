// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content.browser.input.ImeMetricsUtils.ExtensionFormat;

/** Unit tests for {@link ImeMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImeMetricsUtilsTest {

    @Test
    public void testExtensionToFormat() {
        Assert.assertEquals(ExtensionFormat.PNG, ImeMetricsUtils.extensionToFormat("png"));
        Assert.assertEquals(ExtensionFormat.PNG, ImeMetricsUtils.extensionToFormat("PNG"));
        Assert.assertEquals(ExtensionFormat.JPG, ImeMetricsUtils.extensionToFormat("jpg"));
        Assert.assertEquals(ExtensionFormat.JPEG, ImeMetricsUtils.extensionToFormat("jpeg"));
        Assert.assertEquals(ExtensionFormat.GIF, ImeMetricsUtils.extensionToFormat("gif"));
        Assert.assertEquals(ExtensionFormat.SVG, ImeMetricsUtils.extensionToFormat("svg"));
        Assert.assertEquals(ExtensionFormat.WEBP, ImeMetricsUtils.extensionToFormat("webp"));
        Assert.assertEquals(ExtensionFormat.OTHER, ImeMetricsUtils.extensionToFormat("unknown"));
        Assert.assertEquals(ExtensionFormat.OTHER, ImeMetricsUtils.extensionToFormat(null));
        Assert.assertEquals(ExtensionFormat.OTHER, ImeMetricsUtils.extensionToFormat(""));
    }

    @Test
    public void testRecordCommitContentSuccess() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Input.CommitContent.Success", ExtensionFormat.PNG)
                        .expectIntRecord("Input.CommitContent.Failure", ExtensionFormat.OTHER)
                        .build();

        ImeMetricsUtils.recordCommitContentSuccess("png", true);
        ImeMetricsUtils.recordCommitContentSuccess("bmp", false);

        watcher.assertExpected();
    }
}
