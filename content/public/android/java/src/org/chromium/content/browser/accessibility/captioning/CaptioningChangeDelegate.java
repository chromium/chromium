// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.graphics.Color;
import android.graphics.Typeface;
import android.view.accessibility.CaptioningManager.CaptionStyle;

import androidx.annotation.VisibleForTesting;

import org.chromium.content.browser.accessibility.captioning.SystemCaptioningBridge.SystemCaptioningBridgeListener;

import java.lang.ref.WeakReference;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.HashSet;
import java.util.Locale;

/**
 * API level agnostic delegate for getting updates about caption styles.
 *
 * <p>This class is based on CaptioningManager.CaptioningChangeListener except it uses internal
 * classes instead of the API level dependent versions. Here is the documentation for that class:
 *
 * <p>https://developer.android.com/reference/android/view/accessibility/CaptioningManager.CaptioningChangeListener.html
 */
public class CaptioningChangeDelegate {
    private static final String FONT_STYLE_ITALIC = "italic";

    @VisibleForTesting public static final String DEFAULT_CAPTIONING_PREF_VALUE = "";

    private boolean mTextTracksEnabled;

    private String mTextTrackBackgroundColor;
    private String mTextTrackFontFamily;
    private String mTextTrackFontStyle;
    private String mTextTrackFontVariant;
    private String mTextTrackTextColor;
    private String mTextTrackTextShadow;
    private String mTextTrackTextSize;
    // Using weak references to avoid preventing listeners from getting GC'ed.
    private final HashSet<WeakReference<SystemCaptioningBridgeListener>> mListeners =
            new HashSet<>();

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onEnabledChanged
     */
    public void onEnabledChanged(boolean enabled) {
        mTextTracksEnabled = enabled;
        notifySettingsChanged();
    }

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onFontScaleChanged
     */
    public void onFontScaleChanged(float fontScale) {
        mTextTrackTextSize = androidFontScaleToPercentage(fontScale);
        notifySettingsChanged();
    }

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onLocaleChanged
     */
    public void onLocaleChanged(Locale locale) {}

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onUserStyleChanged
     */
    public void onUserStyleChanged(CaptioningStyle userStyle) {
        mTextTrackTextColor = androidColorToCssColor(userStyle.getForegroundColor());
        mTextTrackBackgroundColor = androidColorToCssColor(userStyle.getBackgroundColor());

        mTextTrackTextShadow =
                getShadowFromColorAndSystemEdge(
                        androidColorToCssColor(userStyle.getEdgeColor()), userStyle.getEdgeType());

        final Typeface typeFace = userStyle.getTypeface();
        mTextTrackFontFamily = getFontFromSystemFont(typeFace);
        if (typeFace != null && typeFace.isItalic()) {
            mTextTrackFontStyle = FONT_STYLE_ITALIC;
        } else {
            mTextTrackFontStyle = DEFAULT_CAPTIONING_PREF_VALUE;
        }

        mTextTrackFontVariant = DEFAULT_CAPTIONING_PREF_VALUE;

        notifySettingsChanged();
    }

    /** Construct a new CaptioningChangeDelegate object. */
    public CaptioningChangeDelegate() {}

    /**
     * Get the formatted Text Shadow CSS property from the edge and color attribute.
     *
     * @return the CSS-friendly String representation of the
     *         edge attribute.
     */
    public static String getShadowFromColorAndSystemEdge(String color, Integer type) {
        String edgeShadow = "";
        if (type != null) {
            switch (type) {
                case CaptionStyle.EDGE_TYPE_OUTLINE:
                    edgeShadow =
                            "%2$s %2$s 0 %1$s, -%2$s -%2$s 0 %1$s, %2$s -%2$s 0 %1$s, -%2$s %2$s 0"
                                    + " %1$s";
                    break;
                case CaptionStyle.EDGE_TYPE_DROP_SHADOW:
                    edgeShadow = "%1$s %2$s %2$s 0.1em";
                    break;
                case CaptionStyle.EDGE_TYPE_RAISED:
                    edgeShadow = "-%2$s -%2$s 0 %1$s";
                    break;
                case CaptionStyle.EDGE_TYPE_DEPRESSED:
                    edgeShadow = "%2$s %2$s 0 %1$s";
                    break;
                default:
                    // CaptionStyle.EDGE_TYPE_NONE
                    // CaptionStyle.EDGE_TYPE_UNSPECIFIED
                    break;
            }
        }

        String edgeColor = color;
        if (edgeColor == null || edgeColor.isEmpty()) edgeColor = "silver";

        return String.format(edgeShadow, edgeColor, "0.05em");
    }

    /**
     * Create a font family name based on provided Typeface.
     *
     * @param typeFace a Typeface object.
     * @return a string representation of the font family name.
     */
    public static String getFontFromSystemFont(Typeface typeFace) {
        if (typeFace == null) return "";

        // The list of fonts are obtained from apps/Settings/res/values/arrays.xml
        // in Android settings app.
        String fonts[] = { // Fonts in Lollipop and above
            "", "sans-serif", "sans-serif-condensed", "sans-serif-monospace", "serif",
            "serif-monospace", "casual", "cursive", "sans-serif-smallcaps", "monospace"
        };
        for (String font : fonts) {
            if (Typeface.create(font, typeFace.getStyle()).equals(typeFace)) return font;
        }

        // This includes Typeface.DEFAULT_BOLD since font-weight
        // is not yet supported as a WebKit setting for a VTTCue.
        return "";
    }

    /**
     * Convert an Integer color to a "rgba" CSS style string
     *
     * @param color The Integer color to convert
     * @return a "rgba" CSS style string
     */
    public static String androidColorToCssColor(Integer color) {
        if (color == null) {
            return DEFAULT_CAPTIONING_PREF_VALUE;
        }
        // CSS uses values between 0 and 1 for the alpha level
        final String alpha =
                new DecimalFormat("#.##", new DecimalFormatSymbols(Locale.US))
                        .format(Color.alpha(color) / 255.0);
        // Return a CSS string in the form rgba(r,g,b,a)
        return String.format(
                "rgba(%s, %s, %s, %s)",
                Color.red(color), Color.green(color), Color.blue(color), alpha);
    }

    /**
     * Convert a font scale to a percentage String
     *
     * @param fontScale the font scale value to convert
     * @return a percentage value as a String (eg 50%)
     */
    public static String androidFontScaleToPercentage(float fontScale) {
        return new DecimalFormat("#%", new DecimalFormatSymbols(Locale.US)).format(fontScale);
    }

    private void notifySettingsChanged() {
        for (WeakReference<SystemCaptioningBridgeListener> weakRef : mListeners) {
            SystemCaptioningBridgeListener listener = weakRef.get();
            if (listener != null) {
                notifyListener(listener);
            }
        }
    }

    /**
     * Notify a listener about the current text track settings.
     *
     * @param listener the listener to notify.
     */
    public void notifyListener(SystemCaptioningBridgeListener listener) {
        if (mTextTracksEnabled) {
            final TextTrackSettings settings =
                    new TextTrackSettings(
                            mTextTracksEnabled,
                            mTextTrackBackgroundColor,
                            mTextTrackFontFamily,
                            mTextTrackFontStyle,
                            mTextTrackFontVariant,
                            mTextTrackTextColor,
                            mTextTrackTextShadow,
                            mTextTrackTextSize);
            listener.onSystemCaptioningChanged(settings);
        } else {
            listener.onSystemCaptioningChanged(new TextTrackSettings());
        }
    }

    /**
     * Add a listener for changes with the system CaptioningManager.
     *
     * @param listener The SystemCaptioningBridgeListener object to add.
     */
    public void addListener(SystemCaptioningBridgeListener listener) {
        mListeners.add(new WeakReference<>(listener));
    }

    /**
     * Remove a listener from system CaptionManager.
     *
     * @param listener The SystemCaptioningBridgeListener object to remove.
     */
    public void removeListener(SystemCaptioningBridgeListener listener) {
        // Use an iterator to safely remove weak references to listeners.
        mListeners.removeIf(
                weakRef -> {
                    SystemCaptioningBridgeListener target = weakRef.get();
                    return target == null || target == listener;
                });
    }

    /**
     * Return whether there are listeners associated with this class.
     *
     * @return true if there are at least one listener, or false otherwise.
     */
    public boolean hasActiveListener() {
        return !mListeners.isEmpty();
    }
}
