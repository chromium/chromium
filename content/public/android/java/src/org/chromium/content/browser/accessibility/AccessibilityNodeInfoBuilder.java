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
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_IN_WINDOW_KEY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.ParcelableSpan;
import android.text.SpannableString;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.LocaleSpan;
import android.text.style.StrikethroughSpan;
import android.text.style.StyleSpan;
import android.text.style.SubscriptSpan;
import android.text.style.SuggestionSpan;
import android.text.style.SuperscriptSpan;
import android.text.style.TypefaceSpan;
import android.text.style.URLSpan;
import android.text.style.UnderlineSpan;
import android.view.View;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.ax.mojom.TextPosition;
import org.chromium.ax.mojom.TextStyle;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.accessibility.AccessibilityFeatures;
import org.chromium.ui.accessibility.AccessibilityFeaturesMap;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Basic helper class to build AccessibilityNodeInfo objects for the WebContents in Chrome. This
 * class can be used by the Native-side code {@link web_contents_accessibility_android.cc} to
 * construct objects for the virtual view hierarchy to provide to the Android framework.
 */
@JNINamespace("content")
@NullMarked
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

    public static final String EXTRAS_KEY_REQUEST_LAYOUT_BASED_ACTIONS =
            "AccessibilityNodeInfo.requestLayoutBasedActions";

    public static final String ACCESSIBILITY_SPANNABLE_CREATION_TIME =
            "Accessibility.Android.Performance.SpannableCreationTime2";
    private static final int MAX_TIME_BUCKET = 5 * 1000; // 5,000 microseconds = 5ms.

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
        @Nullable
        String getLanguageTag();

        // Comma separate value of HTML tags that a given node can traverse by.
        @Nullable
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
            boolean clickable,
            boolean contentInvalid,
            boolean enabled,
            boolean editable,
            boolean focusable,
            boolean focused,
            boolean hasImage,
            boolean password,
            boolean scrollable,
            boolean selected,
            boolean visibleToUser,
            boolean hasCharacterLocations,
            boolean isRequired,
            boolean isHeading,
            boolean hasLayoutBasedActions) {
        node.setCheckable(checkable);
        node.setClickable(clickable);
        node.setEditable(editable);
        node.setEnabled(enabled);
        node.setFocusable(focusable);
        node.setFocused(focused);
        node.setPassword(password);
        node.setScrollable(scrollable);
        node.setSelected(selected);
        node.setVisibleToUser(visibleToUser);
        node.setFieldRequired(isRequired);
        node.setContentInvalid(contentInvalid);
        node.setHeading(isHeading);

        List<String> availableExtraData = new ArrayList<>();
        if (hasImage) {
            Bundle bundle = node.getExtras();
            bundle.putCharSequence(EXTRAS_KEY_HAS_IMAGE, "true");
            availableExtraData.add(EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY);
        }

        if (hasCharacterLocations) {
            availableExtraData.add(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
            availableExtraData.add(EXTRA_DATA_TEXT_CHARACTER_LOCATION_IN_WINDOW_KEY);
        }

        if (clickable && !hasLayoutBasedActions) {
            availableExtraData.add(EXTRAS_KEY_REQUEST_LAYOUT_BASED_ACTIONS);
        }

        node.setAvailableExtraData(availableExtraData);

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
            boolean isText,
            boolean enabled,
            boolean editable,
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

        if (hasNonEmptyInnerText) {
            node.addAction(ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
            node.addAction(ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        if (isText && enabled) {
            if (editable) {
                node.addAction(ACTION_SET_TEXT);
                node.addAction(ACTION_PASTE);
                node.addAction(ACTION_IME_ENTER);
            }
            if (hasNonEmptyValue) {
                node.addAction(ACTION_SET_SELECTION);
                if (editable) {
                    node.addAction(ACTION_CUT);
                }
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
            String tooltipText,
            String targetUrl,
            boolean canOpenPopup,
            boolean multiLine,
            int inputType,
            int liveRegion,
            String errorMessage,
            int clickableScore,
            String display,
            String brailleLabel,
            String brailleRoleDescription,
            int expandedState,
            int checked,
            int[] labelledByIds) {
        node.setUniqueId(String.valueOf(virtualViewId));
        node.setClassName(className);

        Bundle bundle = node.getExtras();
        if (!brailleLabel.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_LABEL, brailleLabel);
        }
        if (!brailleRoleDescription.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_ROLE_DESCRIPTION, brailleRoleDescription);
        }
        bundle.putCharSequence(EXTRAS_KEY_CHROME_ROLE, role);

        if (!roleDescription.isEmpty()) {
            node.setRoleDescription(roleDescription);
        }

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
        node.setTooltipText(tooltipText);
        node.setExpandedState(expandedState);

        // If we have enabled WINDOW_CONTENT_CHANGED live region events or deprecated
        // TYPE_ANNOUNCEMENT, we should properly mark live region root nodes. Otherwise, we choose
        // to use AnnounceLiveRegionText() to make this announcement for us.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_DEPRECATE_TYPE_ANNOUNCE)
                || ContentFeatureMap.isEnabled(
                        ContentFeatureList.ACCESSIBILITY_IMPROVE_LIVE_REGION_ANNOUNCE)) {
            node.setLiveRegion(liveRegion);
        }

        // We only apply the |errorMessage| if {@link setAccessibilityNodeInfoBooleanAttributes}
        // set |contentInvalid| to true.
        if (node.isContentInvalid()) {
            node.setError(errorMessage);
        }

        // For non-zero clickable scores, add to the Bundle extras.
        if (clickableScore > 0) {
            bundle.putInt(EXTRAS_KEY_CLICKABLE_SCORE, clickableScore);
        }

        node.setChecked(checked);

        for (int id : labelledByIds) {
            node.addLabeledBy(mDelegate.getView(), id);
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
            String stateDescription,
            String containerTitle,
            String contentDescription,
            String supplementalDescription) {
        long now = SystemClock.elapsedRealtimeNanos() / 1000;

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

        // We add the containerTitle attribute when it is non-null and not empty.
        if (containerTitle != null && !containerTitle.isEmpty()) {
            node.setContainerTitle(containerTitle);
        }

        // We add the contentDescription attribute when it is non-null and not empty.
        if (contentDescription != null && !contentDescription.isEmpty()) {
            node.setContentDescription(contentDescription);
        }

        // We add the supplementalDescription attribute when it is non-null and not empty.
        if (supplementalDescription != null && !supplementalDescription.isEmpty()) {
            node.setSupplementalDescription(supplementalDescription);
        }

        // We expose the nested structure of links, which results in the roles of all nested nodes
        // being read. Use content description in the case of links to prevent verbose TalkBack.
        if (annotateAsLink && (contentDescription == null || contentDescription.isEmpty())) {
            node.setContentDescription(computedText);
        } else {
            node.setText(computedText);

            // Though actions are generally set elsewhere, we make an exception here in order to
            // stay consistent with when we supply `text` on a node. In these cases, we can
            // confidently state there is text selection available via
            // WebContentsAccessibilityAndroid::SetSelection.
            if (computedText.length() > 0
                    && ContentFeatureMap.isEnabled(
                            ContentFeatureList
                                    .ACCESSIBILITY_SET_SELECTABLE_ON_ALL_NODES_WITH_TEXT)) {
                node.addAction(ACTION_SET_SELECTION);
                node.setTextSelectable(true);
            }
        }

        recordTimeToCreateSpannables(now);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoText(
            AccessibilityNodeInfoCompat node,
            String text,
            boolean annotateAsLink,
            boolean isEditableText,
            String stateDescription,
            String containerTitle,
            String contentDescription,
            String supplementalDescription,
            Map<String, int[][]> suggestions,
            Map<String, int[][]> links,
            Map<Float, int[][]> textSizes,
            Map<Integer, int[][]> textStyles,
            Map<Integer, int[][]> textPositions,
            Map<Integer, int[][]> foregroundColors,
            Map<Integer, int[][]> backgroundColors,
            Map<String, int[][]> fontFamilies,
            Map<String, int[][]> locales) {
        assert AccessibilityFeaturesMap.isEnabled(
                        AccessibilityFeatures.ACCESSIBILITY_TEXT_FORMATTING)
                : "setAccessibilityNodeInfoText with text styling information was called when"
                        + " feature was not enabled.";

        long now = SystemClock.elapsedRealtimeNanos() / 1000;

        CharSequence computedText =
                computeText(
                        text,
                        suggestions,
                        links,
                        textSizes,
                        textStyles,
                        textPositions,
                        foregroundColors,
                        backgroundColors,
                        fontFamilies,
                        locales);

        // We add the stateDescription attribute when it is non-null and not empty.
        if (stateDescription != null && !stateDescription.isEmpty()) {
            node.setStateDescription(stateDescription);
        }

        // We add the containerTitle attribute when it is non-null and not empty.
        if (containerTitle != null && !containerTitle.isEmpty()) {
            node.setContainerTitle(containerTitle);
        }

        // We add the contentDescription attribute when it is non-null and not empty.
        if (contentDescription != null && !contentDescription.isEmpty()) {
            node.setContentDescription(contentDescription);
        }

        // We add the supplementalDescription attribute when it is non-null and not empty.
        if (supplementalDescription != null && !supplementalDescription.isEmpty()) {
            node.setSupplementalDescription(supplementalDescription);
        }

        // We expose the nested structure of links, which results in the roles of all nested nodes
        // being read. Use content description in the case of links to prevent verbose TalkBack
        if (annotateAsLink && (contentDescription == null || contentDescription.isEmpty())) {
            node.setContentDescription(computedText);
        } else {
            node.setText(computedText);
        }

        recordTimeToCreateSpannables(now);
    }

    private void recordTimeToCreateSpannables(long startTime) {
        RecordHistogram.recordCustomTimesHistogram(
                ACCESSIBILITY_SPANNABLE_CREATION_TIME,
                (SystemClock.elapsedRealtimeNanos() / 1000) - startTime,
                1,
                MAX_TIME_BUCKET,
                100);
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
                mDelegate.getView(),
                /* isScreenCoordinates= */ true);

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
            AccessibilityNodeInfoCompat node,
            int rowCount,
            int columnCount,
            boolean hierarchical,
            int selectionMode) {
        node.setCollectionInfo(
                AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(
                        rowCount, columnCount, hierarchical, selectionMode));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionItemInfo(
            AccessibilityNodeInfoCompat node,
            int rowIndex,
            int rowSpan,
            int columnIndex,
            int columnSpan) {
        // TODO(crbug.com/443079218): convert to CollectionItemInfo.Builder to remove need for
        // setting
        // heading param.
        node.setCollectionItemInfo(
                AccessibilityNodeInfoCompat.CollectionItemInfoCompat.obtain(
                        rowIndex, rowSpan, columnIndex, columnSpan, /* heading= */ false));
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

        boolean needsSpannable =
                annotateAsLink
                        || (!language.isEmpty() && !language.equals(mDelegate.getLanguageTag()))
                        || (suggestionStarts != null && suggestionStarts.length > 0);

        if (needsSpannable) {
            SpannableString spannable = new SpannableString(text);
            if (annotateAsLink) {
                spannable.setSpan(new URLSpan(targetUrl), 0, spannable.length(), 0);
            }
            if (!language.isEmpty() && !language.equals(mDelegate.getLanguageTag())) {
                Locale locale = Locale.forLanguageTag(language);
                spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            }
            if (suggestionStarts != null && suggestionStarts.length > 0) {
                addSuggestionSpans(spannable, suggestionStarts, suggestionEnds, suggestions);
            }

            return spannable;
        }

        // TODO(mschillaci): Consider if we can remove the `needsSpannable` check above and always
        // return a SpannableString instead of sometimes a String without a performance impact.
        return text;
    }

    private CharSequence computeText(
            String text,
            Map<String, int[][]> suggestions,
            Map<String, int[][]> links,
            Map<Float, int[][]> textSizes,
            Map<Integer, int[][]> textStyles,
            Map<Integer, int[][]> textPositions,
            Map<Integer, int[][]> foregroundColors,
            Map<Integer, int[][]> backgroundColors,
            Map<String, int[][]> fontFamilies,
            Map<String, int[][]> locales) {
        assert AccessibilityFeaturesMap.isEnabled(
                        AccessibilityFeatures.ACCESSIBILITY_TEXT_FORMATTING)
                : "computeText with text styling information was called when feature was not"
                        + " enabled.";

        // We previously would only create a SpannableString if needed, and would check each of
        // these specific cases within a separate if statement. Since every piece of text must have
        // a color, size, background color, etc, we are always making spans so we have removed that
        // extra check and will always return a Spannable.
        SpannableString spannable = new SpannableString(text);
        addSpans(
                spannable,
                suggestions,
                (suggestion) -> {
                    int flags = SuggestionSpan.FLAG_MISSPELLED;
                    return new SuggestionSpan(
                            mDelegate.getContext(), new String[] {suggestion}, flags);
                });
        addSpans(
                spannable,
                links,
                (link) -> {
                    return new URLSpan(link);
                });
        addSpans(
                spannable,
                textSizes,
                (textSize) -> {
                    // TODO: aluh - This is already checked in C++, do we need to check again?
                    // Zero font size is valid in CSS, which makes text invisible.
                    if (textSize >= 0) {
                        return new AbsoluteSizeSpan(Math.round(textSize));
                    }
                    return null;
                });
        addSpans(
                spannable,
                textStyles,
                (textStyle) -> {
                    if (textStyle == TextStyle.BOLD) {
                        return new StyleSpan(Typeface.BOLD);
                    } else if (textStyle == TextStyle.ITALIC) {
                        return new StyleSpan(Typeface.ITALIC);
                    } else if (textStyle == TextStyle.UNDERLINE) {
                        return new UnderlineSpan();
                    } else if (textStyle == TextStyle.LINE_THROUGH) {
                        return new StrikethroughSpan();
                    }
                    return null;
                });
        addSpans(
                spannable,
                textPositions,
                (textPosition) -> {
                    if (textPosition == TextPosition.SUBSCRIPT) {
                        return new SubscriptSpan();
                    } else if (textPosition == TextPosition.SUPERSCRIPT) {
                        return new SuperscriptSpan();
                    }
                    return null;
                });
        addSpans(
                spannable,
                foregroundColors,
                (foregroundColor) -> {
                    return new ForegroundColorSpan(foregroundColor);
                });
        addSpans(
                spannable,
                backgroundColors,
                (backgroundColor) -> {
                    return new BackgroundColorSpan(backgroundColor);
                });
        addSpans(
                spannable,
                fontFamilies,
                (fontFamily) -> {
                    // TODO: aluh - This is already checked in C++, do we need to check again?
                    if (!fontFamily.isEmpty()) {
                        return new TypefaceSpan(fontFamily);
                    }
                    return null;
                });
        addSpans(
                spannable,
                locales,
                (locale) -> {
                    if (!locale.isEmpty() && !locale.equals(mDelegate.getLanguageTag())) {
                        return new LocaleSpan(Locale.forLanguageTag(locale));
                    }
                    return null;
                });

        return spannable;
    }

    private void addSuggestionSpans(
            SpannableString spannable,
            int[] suggestionStarts,
            int[] suggestionEnds,
            String[] suggestions) {
        assert suggestionEnds != null;
        assert suggestionEnds.length == suggestionStarts.length;
        assert suggestions != null;
        assert suggestions.length == suggestionStarts.length;

        for (int i = 0; i < suggestionStarts.length; i++) {
            int start = suggestionStarts[i];
            int end = suggestionEnds[i];
            // Ignore any spans outside the range of the spannable string.
            if (!isRangeInSpannable(spannable, start, end)) {
                continue;
            }

            int flags = SuggestionSpan.FLAG_MISSPELLED;
            SuggestionSpan suggestionSpan =
                    new SuggestionSpan(
                            mDelegate.getContext(), new String[] {suggestions[i]}, flags);
            spannable.setSpan(suggestionSpan, start, end, 0);
        }
    }

    private boolean isValidAttributeRanges(int[][] ranges) {
        return ranges != null
                && ranges.length == 2
                && ranges[0] != null
                && ranges[1] != null
                && ranges[0].length > 0
                && ranges[0].length == ranges[1].length;
    }

    private boolean isRangeInSpannable(SpannableString spannable, int start, int end) {
        return start <= end && start >= 0 && end <= spannable.length();
    }

    @FunctionalInterface
    private interface SpanFactory<T> {
        @Nullable ParcelableSpan createSpan(T param);
    }

    private <T> void addSpans(
            SpannableString spannable, Map<T, int[][]> attributes, SpanFactory<T> spanFactory) {
        if (attributes != null) {
            attributes.forEach(
                    (value, ranges) -> {
                        if (isValidAttributeRanges(ranges)) {
                            for (int i = 0; i < ranges[0].length; i++) {
                                int start = ranges[0][i];
                                int end = ranges[1][i];
                                if (isRangeInSpannable(spannable, start, end)) {
                                    ParcelableSpan span = spanFactory.createSpan(value);
                                    if (span != null) {
                                        spannable.setSpan(span, start, end, 0);
                                    }
                                }
                            }
                        }
                    });
        }
    }

    public static void convertWebRectToAndroidCoordinates(
            Rect rect,
            Bundle extras,
            AccessibilityDelegate.AccessibilityCoordinates accessibilityCoordinates,
            View view,
            boolean isScreenCoordinates) {
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
        // Only offset the view location when the screen coordinates are requested.
        // For window coordinates, no need to offset the view location.
        if (isScreenCoordinates) {
            rect.offset(viewLocation[0], viewLocation[1]);
        }

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
