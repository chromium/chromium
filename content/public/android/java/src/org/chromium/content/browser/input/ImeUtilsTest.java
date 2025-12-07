// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.util.Base64;

@RunWith(JUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ImeUtilsTest {
    @Test
    @SmallTest
    public void testGetDataUrlFromValidInputStream() throws Throwable {
        String base64EncodedData = "UHVDWERNMm4=";
        byte[] imageData = Base64.getDecoder().decode(base64EncodedData);
        InputStream inputStream = new ByteArrayInputStream(imageData);

        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () ->
                        assertEquals(
                                "data:image/png;base64," + base64EncodedData,
                                ImeUtils.getDataUrlFromContentUri(inputStream, "image/png")));
    }

    @Test
    @SmallTest
    public void testGetDataUrlFromNullInputStream() throws Throwable {
        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> assertEquals("", ImeUtils.getDataUrlFromContentUri(null, "image/png")));
    }
}
