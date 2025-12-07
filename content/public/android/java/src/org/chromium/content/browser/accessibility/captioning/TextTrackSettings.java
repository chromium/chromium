// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Bundles the Closed Caption Track Settings and ensures that non-null
 * strings are used used by the recipient of this bundle.
 */
@NullMarked
public final class TextTrackSettings {
    private static final String DEFAULT_VALUE = "";
    private boolean mTextTracksEnabled;
    private @Nullable String mTextTrackBackgroundColor;
    private @Nullable String mTextTrackFontFamily;
    private @Nullable String mTextTrackFontStyle;
    private @Nullable String mTextTrackFontVariant;
    private @Nullable String mTextTrackTextColor;
    private @Nullable String mTextTrackTextShadow;
    private @Nullable String mTextTrackTextSize;

    /**
     * Constructs a new TextTrackSettings object that will
     * return "" for all text track properties and false for textTracksEnabled.
     */
    public TextTrackSettings() {}

    /**
     * Constructs a new TextTrackSettings object
     *
     * @param textTracksEnabled the captions state
     * @param textTrackBackgroundColor the background color
     * @param textTrackFontFamily the font family
     * @param textTrackFontStyle the font style
     * @param textTrackFontVariant the font variant
     * @param textTrackTextColor the text color
     * @param textTrackTextShadow the text shadow
     * @param textTrackTextSize the text size
     */
    public TextTrackSettings(
            boolean textTracksEnabled,
            @Nullable String textTrackBackgroundColor,
            @Nullable String textTrackFontFamily,
            @Nullable String textTrackFontStyle,
            @Nullable String textTrackFontVariant,
            @Nullable String textTrackTextColor,
            @Nullable String textTrackTextShadow,
            @Nullable String textTrackTextSize) {
        mTextTracksEnabled = textTracksEnabled;
        mTextTrackBackgroundColor = textTrackBackgroundColor;
        mTextTrackFontFamily = textTrackFontFamily;
        mTextTrackFontStyle = textTrackFontStyle;
        mTextTrackFontVariant = textTrackFontVariant;
        mTextTrackTextColor = textTrackTextColor;
        mTextTrackTextShadow = textTrackTextShadow;
        mTextTrackTextSize = textTrackTextSize;
    }

    /**
     * @return true if closed captions are "on"
     */
    public boolean getTextTracksEnabled() {
        return mTextTracksEnabled;
    }

    /**
     * @return the text track background color. Will return "" if the
     *         value is null
     */
    public String getTextTrackBackgroundColor() {
        return Objects.toString(mTextTrackBackgroundColor, DEFAULT_VALUE);
    }

    /**
     * @return the text track font family. Will return "" if the
     *         value is null
     */
    public String getTextTrackFontFamily() {
        return Objects.toString(mTextTrackFontFamily, DEFAULT_VALUE);
    }

    /**
     * @return the text track font style. Will return "" if the
     *         value is null
     */
    public String getTextTrackFontStyle() {
        return Objects.toString(mTextTrackFontStyle, DEFAULT_VALUE);
    }

    /**
     * @return the text track font variant. Will return "" if the
     *         value is null
     */
    public String getTextTrackFontVariant() {
        return Objects.toString(mTextTrackFontVariant, DEFAULT_VALUE);
    }

    /**
     * @return the text track text color. Will return "" if the
     *         value is null
     */
    public String getTextTrackTextColor() {
        return Objects.toString(mTextTrackTextColor, DEFAULT_VALUE);
    }

    /**
     * @return the text track text shadow. Will return "" if the
     *         value is null
     */
    public String getTextTrackTextShadow() {
        return Objects.toString(mTextTrackTextShadow, DEFAULT_VALUE);
    }

    /**
     * @return the text track text size. Will return "" if the
     *         value is null
     */
    public String getTextTrackTextSize() {
        return Objects.toString(mTextTrackTextSize, DEFAULT_VALUE);
    }
}
