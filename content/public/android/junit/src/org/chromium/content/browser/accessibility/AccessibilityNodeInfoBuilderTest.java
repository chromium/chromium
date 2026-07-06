// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AccessibilityNodeInfoBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityNodeInfoBuilderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccessibilityNodeInfoBuilder.BuilderDelegate mDelegate;
    @Mock private View mView;

    private AccessibilityNodeInfoBuilder mBuilder;

    @Before
    public void setUp() {
        Mockito.when(mDelegate.getView()).thenReturn(mView);
        Mockito.when(mDelegate.getContext()).thenReturn(ContextUtils.getApplicationContext());
        mBuilder = new AccessibilityNodeInfoBuilder(mDelegate);
    }

    @Test
    @SmallTest
    public void testSetAccessibilityNodeInfoText_LinkWithContentDescription() {
        AccessibilityNodeInfoCompat node = AccessibilityNodeInfoCompat.obtain();

        String text = "Link Text";
        String targetUrl = "http://example.com";
        boolean annotateAsLink = true;
        String contentDescription = "Aria Label";

        mBuilder.setAccessibilityNodeInfoText(
                node,
                text,
                targetUrl,
                annotateAsLink,
                /* language= */ "",
                /* suggestionStarts= */ null,
                /* suggestionEnds= */ null,
                /* suggestions= */ null,
                /* stateDescription= */ "",
                /* containerTitle= */ "",
                contentDescription,
                /* supplementalDescription= */ "");

        // Content description should be set to "Aria Label".
        Assert.assertEquals(contentDescription, node.getContentDescription().toString());

        // Text should be null rather than the computed text since our node is a link.
        Assert.assertNull(node.getText());
    }

    @Test
    @SmallTest
    public void testSetAccessibilityNodeInfoText_LinkWithoutContentDescription() {
        AccessibilityNodeInfoCompat node = AccessibilityNodeInfoCompat.obtain();

        String text = "Link Text";
        String targetUrl = "http://example.com";
        boolean annotateAsLink = true;
        String contentDescription = null;

        mBuilder.setAccessibilityNodeInfoText(
                node,
                text,
                targetUrl,
                annotateAsLink,
                /* language= */ "",
                /* suggestionStarts= */ null,
                /* suggestionEnds= */ null,
                /* suggestions= */ null,
                /* stateDescription= */ "",
                /* containerTitle= */ "",
                contentDescription,
                /* supplementalDescription= */ "");

        // Since we are not provided a valid contentDescription, we should populate our node's
        // contentDescription with computed text.
        Assert.assertNotNull(node.getContentDescription());
        Assert.assertEquals(text, node.getContentDescription().toString());

        // Text should be null rather than the computed text since our node is a link.
        Assert.assertNull(node.getText());
    }

    @Test
    @SmallTest
    public void testSetAccessibilityNodeInfoText_NotLink() {
        AccessibilityNodeInfoCompat node = AccessibilityNodeInfoCompat.obtain();

        String text = "Normal Text";
        String targetUrl = null;
        boolean annotateAsLink = false;
        String contentDescription = null;

        mBuilder.setAccessibilityNodeInfoText(
                node,
                text,
                targetUrl,
                annotateAsLink,
                /* language= */ "",
                /* suggestionStarts= */ null,
                /* suggestionEnds= */ null,
                /* suggestions= */ null,
                /* stateDescription= */ "",
                /* containerTitle= */ "",
                contentDescription,
                /* supplementalDescription= */ "");

        // Content description should be null.
        Assert.assertNull(node.getContentDescription());

        // Text should be set to the computed text.
        Assert.assertNotNull(node.getText());
        Assert.assertEquals(text, node.getText().toString());
    }
}
