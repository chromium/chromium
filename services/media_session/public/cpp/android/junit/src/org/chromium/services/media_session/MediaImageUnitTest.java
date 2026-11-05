// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.media_session;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link MediaImage}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaImageUnitTest {
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;
    private static final GURL TEST_URL_2 = JUnitTestGURLs.URL_2;

    @Test
    public void testGettersAndSetters() {
        List<Rect> sizes = new ArrayList<>();
        sizes.add(new Rect(0, 0, 100, 200));

        MediaImage image = new MediaImage(TEST_URL, "image/png", sizes);
        assertEquals(TEST_URL, image.getSrc());
        assertEquals("image/png", image.getType());
        assertEquals(sizes, image.getSizes());

        image.setSrc(TEST_URL_2);
        image.setType("image/jpeg");
        List<Rect> newSizes = new ArrayList<>();
        newSizes.add(new Rect(0, 0, 300, 400));
        image.setSizes(newSizes);

        assertEquals(TEST_URL_2, image.getSrc());
        assertEquals("image/jpeg", image.getType());
        assertEquals(newSizes, image.getSizes());
    }

    @Test
    public void testGetSizesFlatArray() {
        MediaImage emptyImage = new MediaImage(TEST_URL, "image/png", new ArrayList<>());
        assertArrayEquals(new int[0], emptyImage.getSizesFlatArray());

        List<Rect> sizes =
                Arrays.asList(
                        new Rect(0, 0, 100, 150),
                        new Rect(0, 0, 200, 250),
                        new Rect(10, 20, 310, 420));
        MediaImage image = new MediaImage(TEST_URL, "image/png", sizes);
        int[] expected = new int[] {100, 150, 200, 250, 300, 400};
        assertArrayEquals(expected, image.getSizesFlatArray());
    }

    @Test
    public void testEqualsAndHashCode() {
        List<Rect> sizes1 = Arrays.asList(new Rect(0, 0, 100, 100));
        List<Rect> sizes2 = Arrays.asList(new Rect(0, 0, 100, 100));
        List<Rect> differentSizes = Arrays.asList(new Rect(0, 0, 200, 200));

        MediaImage image1 = new MediaImage(TEST_URL, "image/png", sizes1);
        MediaImage image2 = new MediaImage(TEST_URL, "image/png", sizes2);
        MediaImage diffUrl = new MediaImage(TEST_URL_2, "image/png", sizes1);
        MediaImage diffType = new MediaImage(TEST_URL, "image/jpeg", sizes1);
        MediaImage diffSize = new MediaImage(TEST_URL, "image/png", differentSizes);

        assertEquals(image1, image2);
        assertEquals(image1.hashCode(), image2.hashCode());

        assertNotEquals(image1, diffUrl);
        assertNotEquals(image1, diffType);
        assertNotEquals(image1, diffSize);
        assertFalse(image1.equals(null));
        assertFalse(image1.equals(new Object()));
    }
}
