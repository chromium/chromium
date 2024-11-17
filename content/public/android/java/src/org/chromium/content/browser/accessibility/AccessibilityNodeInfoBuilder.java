// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CONTEXT_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COPY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CUT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_EXPAND;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_IME_ENTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_LONG_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PASTE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_PROGRESS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_SELECTION;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_TEXT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SHOW_ON_SCREEN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.style.LocaleSpan;
import android.text.style.SuggestionSpan;
import android.text.style.URLSpan;
import android.view.View;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * Basic helper class to build AccessibilityNodeInfo objects for the WebContents in Chrome. This
 * class can be used by the Native-side code {@link web_contents_accessibility_android.cc} to
 * construct objects for the virtual view hierarchy to provide to the Android framework.
 */
@JNINamespace("content")
public class AccessibilityNodeInfoBuilder {
    // Constants defined for AccessibilityNodeInfo Bundle extras keys. These values are Chromium
    // specific, and allow Chromium-based browsers to provide richer information to AT. These
    // are added to every relevant node (e.g. hasImage will always be present in the Bundle for
    // AccessibilityNodeInfo objects that represent images in the web contents).
    public static final String EXTRAS_KEY_BRAILLE_LABEL = "AccessibilityNodeInfo.brailleLabel";
    public static final String EXTRAS_KEY_BRAILLE_ROLE_DESCRIPTION =
            "AccessibilityNodeInfo.brailleRoleDescription";
    public static final String EXTRAS_KEY_CHROME_ROLE = "AccessibilityNodeInfo.chromeRole";
    public static final String EXTRAS_KEY_CLICKABLE_SCORE = "AccessibilityNodeInfo.clickableScore";
    public static final String EXTRAS_KEY_CSS_DISPLAY = "AccessibilityNodeInfo.cssDisplay";
    public static final String EXTRAS_KEY_HAS_IMAGE = "AccessibilityNodeInfo.hasImage";
    public static final String EXTRAS_KEY_HINT = "AccessibilityNodeInfo.hint";
    public static final String EXTRAS_KEY_OFFSCREEN = "AccessibilityNodeInfo.offscreen";
    public static final String EXTRAS_KEY_ROLE_DESCRIPTION =
            "AccessibilityNodeInfo.roleDescription";
    public static final String EXTRAS_KEY_SUPPORTED_ELEMENTS =
            "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES";
    public static final String EXTRAS_KEY_TARGET_URL = "AccessibilityNodeInfo.targetUrl";

    // Keys used for Bundle extras of parent relative bounds values, without screen clipping.
    public static final String EXTRAS_KEY_UNCLIPPED_TOP = "AccessibilityNodeInfo.unclippedTop";
    public static final String EXTRAS_KEY_UNCLIPPED_BOTTOM =
            "AccessibilityNodeInfo.unclippedBottom";
    public static final String EXTRAS_KEY_UNCLIPPED_LEFT = "AccessibilityNodeInfo.unclippedLeft";
    public static final String EXTRAS_KEY_UNCLIPPED_RIGHT = "AccessibilityNodeInfo.unclippedRight";
    public static final String EXTRAS_KEY_UNCLIPPED_WIDTH = "AccessibilityNodeInfo.unclippedWidth";
    public static final String EXTRAS_KEY_UNCLIPPED_HEIGHT =
            "AccessibilityNodeInfo.unclippedHeight";

    // Keys used for Bundle extras of page absolute bounds values, without screen clipping.
    public static final String EXTRAS_KEY_PAGE_ABSOLUTE_LEFT =
            "AccessibilityNodeInfo.pageAbsoluteLeft";
    public static final String EXTRAS_KEY_PAGE_ABSOLUTE_TOP =
            "AccessibilityNodeInfo.pageAbsoluteTop";
    public static final String EXTRAS_KEY_PAGE_ABSOLUTE_WIDTH =
            "AccessibilityNodeInfo.pageAbsoluteWidth";
    public static final String EXTRAS_KEY_PAGE_ABSOLUTE_HEIGHT =
            "AccessibilityNodeInfo.pageAbsoluteHeight";

    public static final String EXTRAS_KEY_URL = "url";

    // Constants defined for requests to add extra data to AccessibilityNodeInfo objects. These
    // values are Chromium specific, and allow AT to request extra data be added to the node
    // objects. These are used for large/async requests and are done on a per-node basis.
    public static final String EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY =
            "AccessibilityNodeInfo.requestImageData";
    public static final String EXTRAS_KEY_IMAGE_DATA = "AccessibilityNodeInfo.imageData";

    // Static instances of the two types of extra data keys that can be added to nodes.
    private static final List<String> sTextCharacterLocation =
            Collections.singletonList(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);

    private static final List<String> sRequestImageData =
            Collections.singletonList(EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY);

    // Throttle time for content invalid utterances. Content invalid will only be announced at most
    // once per this time interval in milliseconds for a given focused node.
    private static final int CONTENT_INVALID_THROTTLE_DELAY = 4500;

    // These track the last focused content invalid view id and the last time we reported content
    // invalid for that node. Used to ensure we report content invalid on a node once per interval.
    private int mLastContentInvalidViewId;
    private long mLastContentInvalidUtteranceTime;

    /** Delegate interface for any client that wants to use the node builder. */
    interface BuilderDelegate {
        // The view that contains the content this builder is used for.
        View getView();

        // The context for the view that contains the content this builder is used for.
        Context getContext();

        // The virtualViewId of the root of the view that contains the content for this builder.
        int currentRootId();

        // The virtualViewId of the node that currently has accessibility focus inside the View.
        int currentAccessibilityFocusId();

        // The language tag String provided by the default Locale of the device.
        String getLanguageTag();

        // Comma separate value of HTML tags that a given node can traverse by.
        String getSupportedHtmlTags();

        // Set of coordinates for providing the correct size and scroll of the View.
        AccessibilityDelegate.AccessibilityCoordinates getAccessibilityCoordinates();
    }

    public final BuilderDelegate mDelegate;

    protected AccessibilityNodeInfoBuilder(BuilderDelegate delegate) {
        this.mDelegate = delegate;
    }

    @CalledByNative
    private void addAccessibilityNodeInfoChildren(
            AccessibilityNodeInfoCompat node, int[] childIds) {
        for (int childId : childIds) {
            node.addChild(mDelegate.getView(), childId);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBooleanAttributes(
            AccessibilityNodeInfoCompat node,
            int virtualViewId,
            boolean checkable,
            boolean checked,
            boolean clickable,
            boolean contentInvalid,
            boolean enabled,
            boolean focusable,
            boolean focused,
            boolean hasImage,
            boolean password,
            boolean scrollable,
            boolean selected,
            boolean visibleToUser,
            boolean hasCharacterLocations) {
        node.setCheckable(checkable);
        node.setChecked(checked);
        node.setClickable(clickable);
        node.setEnabled(enabled);
        node.setFocusable(focusable);
        node.setFocused(focused);
        node.setPassword(password);
        node.setScrollable(scrollable);
        node.setSelected(selected);
        node.setVisibleToUser(visibleToUser);

        // In the special case that we have invalid content on a focused field, we only want to
        // report that to the user at most once per {@link CONTENT_INVALID_THROTTLE_DELAY} time
        // interval, to be less jarring to the user.
        if (contentInvalid && focused) {
            if (virtualViewId == mLastContentInvalidViewId) {
                // If we are focused on the same node as before, check if it has been longer than
                // our delay since our last utterance, and if so, report invalid content and update
                // our last reported time, otherwise suppress reporting content invalid.
                if (SystemClock.elapsedRealtime() - mLastContentInvalidUtteranceTime
                        >= CONTENT_INVALID_THROTTLE_DELAY) {
                    mLastContentInvalidUtteranceTime = SystemClock.elapsedRealtime();
                    node.setContentInvalid(true);
                }
            } else {
                // When we are focused on a new node, report as normal and track new time.
                mLastContentInvalidViewId = virtualViewId;
                mLastContentInvalidUtteranceTime = SystemClock.elapsedRealtime();
                node.setContentInvalid(true);
            }
        } else {
            // For non-focused fields we want to set contentInvalid as normal.
            node.setContentInvalid(contentInvalid);
        }

        if (hasImage) {
            Bundle bundle = node.getExtras();
            bundle.putCharSequence(EXTRAS_KEY_HAS_IMAGE, "true");
            node.setAvailableExtraData(sRequestImageData);
        }

        if (hasCharacterLocations) {
            node.setAvailableExtraData(sTextCharacterLocation);
        }

        node.setMovementGranularities(
                MOVEMENT_GRANULARITY_CHARACTER
                        | MOVEMENT_GRANULARITY_WORD
                        | MOVEMENT_GRANULARITY_LINE
                        | MOVEMENT_GRANULARITY_PARAGRAPH);

        boolean isAF = mDelegate.currentAccessibilityFocusId() == virtualViewId;
        node.setAccessibilityFocused(isAF);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoActions(
            AccessibilityNodeInfoCompat node,
            int virtualViewId,
            boolean canScrollForward,
            boolean canScrollBackward,
            boolean canScrollUp,
            boolean canScrollDown,
            boolean canScrollLeft,
            boolean canScrollRight,
            boolean clickable,
            boolean editableText,
            boolean enabled,
            boolean focusable,
            boolean focused,
            boolean isCollapsed,
            boolean isExpanded,
            boolean hasNonEmptyValue,
            boolean hasNonEmptyInnerText,
            boolean isSeekControl,
            boolean unused_isForm) {
        node.addAction(ACTION_NEXT_HTML_ELEMENT);
        node.addAction(ACTION_PREVIOUS_HTML_ELEMENT);
        node.addAction(ACTION_SHOW_ON_SCREEN);
        node.addAction(ACTION_CONTEXT_CLICK);

        // We choose to not add ACTION_LONG_CLICK to nodes to prevent verbose utterances, unless
        // the relevant experiment is enabled.
        if (ContentFeatureMap.isEnabled(
                ContentFeatureList.ACCESSIBILITY_INCLUDE_LONG_CLICK_ACTION)) {
            node.addAction(ACTION_LONG_CLICK);
        }

        if (hasNonEmptyInnerText) {
            node.addAction(ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
            node.addAction(ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        if (editableText && enabled) {
            // TODO: don't support actions that modify it if it's read-only (but
            // SET_SELECTION and COPY are okay).
            node.addAction(ACTION_SET_TEXT);
            node.addAction(ACTION_PASTE);
            node.addAction(ACTION_IME_ENTER);

            if (hasNonEmptyValue) {
                node.addAction(ACTION_SET_SELECTION);
                node.addAction(ACTION_CUT);
                node.addAction(ACTION_COPY);
            }
        }

        if (canScrollForward) {
            node.addAction(ACTION_SCROLL_FORWARD);
        }

        if (canScrollBackward) {
            node.addAction(ACTION_SCROLL_BACKWARD);
        }

        if (canScrollUp) {
            node.addAction(ACTION_SCROLL_UP);
            node.addAction(ACTION_PAGE_UP);
        }

        if (canScrollDown) {
            node.addAction(ACTION_SCROLL_DOWN);
            node.addAction(ACTION_PAGE_DOWN);
        }

        if (canScrollLeft) {
            node.addAction(ACTION_SCROLL_LEFT);
            node.addAction(ACTION_PAGE_LEFT);
        }

        if (canScrollRight) {
            node.addAction(ACTION_SCROLL_RIGHT);
            node.addAction(ACTION_PAGE_RIGHT);
        }

        if (focusable) {
            if (focused) {
                node.addAction(ACTION_CLEAR_FOCUS);
            } else {
                node.addAction(ACTION_FOCUS);
            }
        }

        if (mDelegate.currentAccessibilityFocusId() == virtualViewId) {
            node.addAction(ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        } else {
            node.addAction(ACTION_ACCESSIBILITY_FOCUS);
        }

        if (clickable) {
            node.addAction(ACTION_CLICK);
        }

        if (isCollapsed) {
            node.addAction(ACTION_EXPAND);
        }

        if (isExpanded) {
            node.addAction(ACTION_COLLAPSE);
        }

        if (isSeekControl) {
            node.addAction(ACTION_SET_PROGRESS);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBaseAttributes(
            AccessibilityNodeInfoCompat node,
            int virtualViewId,
            int parentId,
            String className,
            String role,
            String roleDescription,
            String hint,
            String targetUrl,
            boolean canOpenPopup,
            boolean multiLine,
            int inputType,
            int unused_liveRegion,
            String errorMessage,
            int clickableScore,
            String display,
            String brailleLabel,
            String brailleRoleDescription) {
        node.setClassName(className);

        Bundle bundle = node.getExtras();
        if (!brailleLabel.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_LABEL, brailleLabel);
        }
        if (!brailleRoleDescription.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_ROLE_DESCRIPTION, brailleRoleDescription);
        }
        bundle.putCharSequence(EXTRAS_KEY_CHROME_ROLE, role);
        bundle.putCharSequence(EXTRAS_KEY_ROLE_DESCRIPTION, roleDescription);
        // We added the hint Bundle extra pre Android-O, and keep it to not risk breaking changes.
        bundle.putCharSequence(EXTRAS_KEY_HINT, hint);
        if (!display.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_CSS_DISPLAY, display);
        }
        if (!targetUrl.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_TARGET_URL, targetUrl);
        }
        if (virtualViewId == mDelegate.currentRootId()) {
            bundle.putCharSequence(EXTRAS_KEY_SUPPORTED_ELEMENTS, mDelegate.getSupportedHtmlTags());
        }

        if (parentId != View.NO_ID) {
            node.setParent(mDelegate.getView(), parentId);
        }

        node.setCanOpenPopup(canOpenPopup);
        node.setDismissable(false); // No concept of "dismissable" on the web currently.
        node.setMultiLine(multiLine);
        node.setInputType(inputType);
        node.setHintText(hint);

        // Deliberately don't call setLiveRegion because TalkBack speaks
        // the entire region anytime it changes. Instead Chrome will
        // call announceLiveRegionText() only on the nodes that change.
        // node.setLiveRegion(liveRegion);

        // We only apply the |errorMessage| if {@link setAccessibilityNodeInfoBooleanAttributes}
        // set |contentInvalid| to true based on throttle delay.
        if (node.isContentInvalid()) {
            node.setError(errorMessage);
        }

        // For non-zero clickable scores, add to the Bundle extras.
        if (clickableScore > 0) {
            bundle.putInt(EXTRAS_KEY_CLICKABLE_SCORE, clickableScore);
        }
    }

    @SuppressLint("NewApi")
    @CalledByNative
    protected void setAccessibilityNodeInfoText(
            AccessibilityNodeInfoCompat node,
            String text,
            String targetUrl,
            boolean annotateAsLink,
            boolean isEditableText,
            String language,
            int[] suggestionStarts,
            int[] suggestionEnds,
            String[] suggestions,
            String stateDescription) {
        CharSequence computedText =
                computeText(
                        text,
                        targetUrl,
                        annotateAsLink,
                        language,
                        suggestionStarts,
                        suggestionEnds,
                        suggestions);

        // We add the stateDescription attribute when it is non-null and not empty.
        if (stateDescription != null && !stateDescription.isEmpty()) {
            node.setStateDescription(stateDescription);
        }

        // We expose the nested structure of links, which results in the roles of all nested nodes
        // being read. Use content description in the case of links to prevent verbose TalkBack
        if (annotateAsLink) {
            node.setContentDescription(computedText);
        } else {
            node.setText(computedText);
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoLocation(
            AccessibilityNodeInfoCompat node,
            final int virtualViewId,
            int absoluteLeft,
            int absoluteTop,
            int parentRelativeLeft,
            int parentRelativeTop,
            int width,
            int height,
            boolean isOffscreen) {
        // First set the bounds in parent.
        Rect boundsInParent =
                new Rect(
                        parentRelativeLeft,
                        parentRelativeTop,
                        parentRelativeLeft + width,
                        parentRelativeTop + height);
        if (virtualViewId == mDelegate.currentRootId()) {
            // Offset of the web content relative to the View.
            AccessibilityDelegate.AccessibilityCoordinates ac =
                    mDelegate.getAccessibilityCoordinates();
            boundsInParent.offset(0, (int) ac.getContentOffsetYPix());
        }
        node.setBoundsInParent(boundsInParent);

        Rect rect = new Rect(absoluteLeft, absoluteTop, absoluteLeft + width, absoluteTop + height);
        convertWebRectToAndroidCoordinates(
                rect,
                node.getExtras(),
                mDelegate.getAccessibilityCoordinates(),
                mDelegate.getView());

        node.setBoundsInScreen(rect);

        // For nodes that are considered visible to the user, but are offscreen (because they are
        // scrolled offscreen or obscured from view but not programmatically hidden, e.g. through
        // CSS), add to the extras Bundle to inform interested accessibility services.
        if (isOffscreen) {
            node.getExtras().putBoolean(EXTRAS_KEY_OFFSCREEN, true);
        } else {
            // In case of a cached node, remove the offscreen extra if it is there.
            if (node.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN)) {
                node.getExtras().remove(EXTRAS_KEY_OFFSCREEN);
            }
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfoCompat node, int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(
                AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(
                        rowCount, columnCount, hierarchical));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionItemInfo(
            AccessibilityNodeInfoCompat node,
            int rowIndex,
            int rowSpan,
            int columnIndex,
            int columnSpan,
            boolean heading) {
        node.setCollectionItemInfo(
                AccessibilityNodeInfoCompat.CollectionItemInfoCompat.obtain(
                        rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfoCompat node, int rangeType, float min, float max, float current) {
        node.setRangeInfo(
                AccessibilityNodeInfoCompat.RangeInfoCompat.obtain(rangeType, min, max, current));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfoCompat node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoPaneTitle(
            AccessibilityNodeInfoCompat node, String title) {
        node.setPaneTitle(title);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoSelectionAttrs(
            AccessibilityNodeInfoCompat node, int startIndex, int endIndex) {
        node.setEditable(true);
        node.setTextSelection(startIndex, endIndex);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoImageData(
            AccessibilityNodeInfoCompat info, byte[] imageData) {
        info.getExtras().putByteArray(EXTRAS_KEY_IMAGE_DATA, imageData);
    }

    private CharSequence computeText(
            String text,
            String targetUrl,
            boolean annotateAsLink,
            String language,
            int[] suggestionStarts,
            int[] suggestionEnds,
            String[] suggestions) {
        CharSequence charSequence = text;
        if (annotateAsLink) {
            SpannableString spannable = new SpannableString(text);
            spannable.setSpan(new URLSpan(targetUrl), 0, spannable.length(), 0);
            charSequence = spannable;
        }
        if (!language.isEmpty() && !language.equals(mDelegate.getLanguageTag())) {
            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }
            Locale locale = Locale.forLanguageTag(language);
            spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            charSequence = spannable;
        }

        if (suggestionStarts != null && suggestionStarts.length > 0) {
            assert suggestionEnds != null;
            assert suggestionEnds.length == suggestionStarts.length;
            assert suggestions != null;
            assert suggestions.length == suggestionStarts.length;

            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }

            int spannableLen = spannable.length();
            for (int i = 0; i < suggestionStarts.length; i++) {
                int start = suggestionStarts[i];
                int end = suggestionEnds[i];
                // Ignore any spans outside the range of the spannable string.
                if (start < 0
                        || start > spannableLen
                        || end < 0
                        || end > spannableLen
                        || start > end) {
                    continue;
                }

                String[] suggestionArray = new String[1];
                suggestionArray[0] = suggestions[i];
                int flags = SuggestionSpan.FLAG_MISSPELLED;
                SuggestionSpan suggestionSpan =
                        new SuggestionSpan(mDelegate.getContext(), suggestionArray, flags);
                spannable.setSpan(suggestionSpan, start, end, 0);
            }
            charSequence = spannable;
        }

        return charSequence;
    }

    public static void convertWebRectToAndroidCoordinates(
            Rect rect,
            Bundle extras,
            AccessibilityDelegate.AccessibilityCoordinates accessibilityCoordinates,
            View view) {
        // Offset by the scroll position.
        AccessibilityDelegate.AccessibilityCoordinates ac = accessibilityCoordinates;
        rect.offset(-(int) ac.getScrollX(), -(int) ac.getScrollY());

        // Convert CSS (web) pixels to Android View pixels
        rect.left = (int) ac.fromLocalCssToPix(rect.left);
        rect.top = (int) ac.fromLocalCssToPix(rect.top);
        rect.bottom = (int) ac.fromLocalCssToPix(rect.bottom);
        rect.right = (int) ac.fromLocalCssToPix(rect.right);

        // Offset by the location of the web content within the view.
        rect.offset(0, (int) ac.getContentOffsetYPix());

        // Finally offset by the location of the view within the screen.
        final int[] viewLocation = new int[2];
        view.getLocationOnScreen(viewLocation);
        rect.offset(viewLocation[0], viewLocation[1]);

        // TODO(mschillaci): This block is the same per-node and is purely viewport dependent,
        //                   pull this out into a reusable object for simplicity/performance.
        // rect is the unclipped values, but we need to clip to viewport bounds. The original
        // unclipped values will be placed in the Bundle extras.
        int clippedTop = viewLocation[1] + (int) ac.getContentOffsetYPix();
        int clippedBottom = clippedTop + ac.getLastFrameViewportHeightPixInt();
        // There is currently no x offset, y offset comes from tab bar / browser controls.
        int clippedLeft = viewLocation[0];
        int clippedRight = clippedLeft + ac.getLastFrameViewportWidthPixInt();

        // Always provide the unclipped bounds in the Bundle for any interested downstream client.
        extras.putInt(EXTRAS_KEY_UNCLIPPED_TOP, rect.top);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_BOTTOM, rect.bottom);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_LEFT, rect.left);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_RIGHT, rect.right);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_WIDTH, rect.width());
        extras.putInt(EXTRAS_KEY_UNCLIPPED_HEIGHT, rect.height());

        if (rect.top < clippedTop) {
            rect.top = clippedTop;
        } else if (rect.top > clippedBottom) {
            rect.top = clippedBottom;
        }

        if (rect.bottom > clippedBottom) {
            rect.bottom = clippedBottom;
        } else if (rect.bottom < clippedTop) {
            rect.bottom = clippedTop;
        }

        if (rect.left < clippedLeft) {
            rect.left = clippedLeft;
        } else if (rect.left > clippedRight) {
            rect.left = clippedRight;
        }

        if (rect.right > clippedRight) {
            rect.right = clippedRight;
        } else if (rect.right < clippedLeft) {
            rect.right = clippedLeft;
        }
    }
}
