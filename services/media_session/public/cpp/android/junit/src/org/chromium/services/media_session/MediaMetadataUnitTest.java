// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.media_session;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link MediaMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaMetadataUnitTest {
    @Test
    public void testDefaultConstructor() {
        MediaMetadata metadata = new MediaMetadata("title", "artist", "album");
        assertEquals("title", metadata.getTitle());
        assertEquals("artist", metadata.getArtist());
        assertEquals("album", metadata.getAlbum());
        assertEquals("", metadata.getSourceTitle());
    }

    @Test
    public void testConstructorWithSourceTitle() {
        MediaMetadata metadata = new MediaMetadata("title", "artist", "album", "sourceTitle");
        assertEquals("title", metadata.getTitle());
        assertEquals("artist", metadata.getArtist());
        assertEquals("album", metadata.getAlbum());
        assertEquals("sourceTitle", metadata.getSourceTitle());
    }

    @Test
    public void testSetters() {
        MediaMetadata metadata = new MediaMetadata("title", "artist", "album", "sourceTitle");
        metadata.setTitle("newTitle");
        metadata.setArtist("newArtist");
        metadata.setAlbum("newAlbum");

        assertEquals("newTitle", metadata.getTitle());
        assertEquals("newArtist", metadata.getArtist());
        assertEquals("newAlbum", metadata.getAlbum());

        metadata.setTitle(null);
        assertNull(metadata.getTitle());
    }

    @Test
    public void testEqualsAndHashCode() {
        MediaMetadata metadata1 = new MediaMetadata("title", "artist", "album", "sourceTitle");
        MediaMetadata metadata2 = new MediaMetadata("title", "artist", "album", "sourceTitle");
        MediaMetadata differentSourceTitle =
                new MediaMetadata("title", "artist", "album", "otherSource");
        MediaMetadata differentTitle =
                new MediaMetadata("otherTitle", "artist", "album", "sourceTitle");
        MediaMetadata differentArtist =
                new MediaMetadata("title", "otherArtist", "album", "sourceTitle");
        MediaMetadata differentAlbum =
                new MediaMetadata("title", "artist", "otherAlbum", "sourceTitle");

        assertTrue(metadata1.equals(metadata1));
        assertTrue(metadata1.equals(metadata2));
        assertEquals(metadata1.hashCode(), metadata2.hashCode());

        MediaMetadata nullTitle1 = new MediaMetadata(null, "artist", "album", "sourceTitle");
        MediaMetadata nullTitle2 = new MediaMetadata(null, "artist", "album", "sourceTitle");
        assertTrue(nullTitle1.equals(nullTitle2));
        assertEquals(nullTitle1.hashCode(), nullTitle2.hashCode());
        assertFalse(metadata1.equals(nullTitle1));
        assertFalse(nullTitle1.equals(metadata1));

        assertFalse(metadata1.equals(differentSourceTitle));
        assertFalse(metadata1.equals(differentTitle));
        assertFalse(metadata1.equals(differentArtist));
        assertFalse(metadata1.equals(differentAlbum));
        assertFalse(metadata1.equals(null));
        assertFalse(metadata1.equals("string"));
    }
}
