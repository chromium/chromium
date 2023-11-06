// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.inputmethod.EditorInfo;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

/** IME (input method editor) and text input tests. */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(ImeTest.IME_BATCH)
public class ImeAutocapitalizeTest {
    static final String AUTOCAPITALIZE_HTML = "content/test/data/android/input/autocapitalize.html";

    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(AUTOCAPITALIZE_HTML);
    }

    @After
    public void tearDown() throws Exception {
        mRule.getActivity().finish();
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testAutocapitalizeAttribute() throws Throwable {
        final int autocapitalizeFlagMask =
                EditorInfo.TYPE_TEXT_FLAG_CAP_CHARACTERS
                        | EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES
                        | EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS;

        // <input> element without autocapitalize attribute set. Should enable sentences
        // autocapitalization as the default behavior.
        mRule.focusElement("input_text");
        Assert.assertEquals(
                EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES,
                mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // <input> element that has autocapitalize="none" set.
        mRule.focusElement("input_autocapitalize_none");
        Assert.assertEquals(
                0, mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // <input> element that has autocapitalize="characters" set.
        mRule.focusElement("input_autocapitalize_characters");
        Assert.assertEquals(
                EditorInfo.TYPE_TEXT_FLAG_CAP_CHARACTERS,
                mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // <input> element that has autocapitalize="words" set.
        mRule.focusElement("input_autocapitalize_words");
        Assert.assertEquals(
                EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS,
                mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // <input> element that has autocapitalize="sentences" set.
        mRule.focusElement("input_autocapitalize_sentences");
        Assert.assertEquals(
                EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES,
                mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // <input> element that inherits autocapitalize="none" from its parent <form> element.
        mRule.focusElement("input_autocapitalize_inherit_none");
        Assert.assertEquals(
                0, mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);

        // contenteditable <div> element with autocapitalize="characters".
        mRule.focusElement("div_autocapitalize_characters");
        Assert.assertEquals(
                EditorInfo.TYPE_TEXT_FLAG_CAP_CHARACTERS,
                mRule.getConnectionFactory().getOutAttrs().inputType & autocapitalizeFlagMask);
    }
}
