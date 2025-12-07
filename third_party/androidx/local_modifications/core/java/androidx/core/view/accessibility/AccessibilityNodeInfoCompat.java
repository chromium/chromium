/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.core.view.accessibility;

import static android.view.View.NO_ID;

import static androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP_PREFIX;

import static java.util.Collections.emptyList;

import android.accessibilityservice.AccessibilityService;
import android.annotation.SuppressLint;
import android.content.ClipData;
import android.graphics.Rect;
import android.graphics.Region;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.Selection;
import android.view.accessibility.AccessibilityNodeInfo.SelectionPosition;
import android.view.accessibility.AccessibilityNodeInfo.TouchDelegateInfo;

import androidx.annotation.IntDef;
import androidx.annotation.IntRange;
import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;
import androidx.annotation.RestrictTo;
import androidx.core.R;
import androidx.core.accessibilityservice.AccessibilityServiceInfoCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityViewCommand.CommandArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.MoveAtGranularityArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.MoveHtmlArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.MoveWindowArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.ScrollToPositionArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.SetProgressArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.SetSelectionArguments;
import androidx.core.view.accessibility.AccessibilityViewCommand.SetTextArguments;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Helper for accessing {@link android.view.accessibility.AccessibilityNodeInfo} in a backwards
 * compatible fashion.
 */
public class AccessibilityNodeInfoCompat {

    private static boolean isAtLeastB_1() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA
                && Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1;
    }

    /**
     * A class defining an action that can be performed on an {@link AccessibilityNodeInfo}. Each
     * action has a unique id and a label.
     *
     * <p>There are three categories of actions:
     *
     * <ul>
     *   <li><strong>Standard actions</strong> - These are actions that are reported and handled by
     *       the standard UI widgets in the platform. Each standard action is associated with a
     *       resource id, e.g. {@link android.R.id#accessibilityActionScrollUp}. Note that actions
     *       were formerly associated with static constants defined in this class, e.g. {@link
     *       #ACTION_FOCUS}. These actions will have {@code null} labels.
     *   <li><strong>Custom actions action</strong> - These are actions that are reported and
     *       handled by custom widgets. i.e. ones that are not part of the UI toolkit. For example,
     *       an application may define a custom action for clearing the user history.
     *   <li><strong>Overriden standard actions</strong> - These are actions that override standard
     *       actions to customize them. For example, an app may add a label to the standard {@link
     *       #ACTION_CLICK} action to indicate to the user that this action clears browsing history.
     * </ul>
     *
     * <p class="note"><strong>Note:</strong> Views which support these actions should invoke {@link
     * ViewCompat#setImportantForAccessibility(View, int)} with {@link
     * ViewCompat#IMPORTANT_FOR_ACCESSIBILITY_YES} to ensure an {@link
     * android.accessibilityservice.AccessibilityService} can discover the set of supported actions.
     */
    public static class AccessibilityActionCompat {

        private static final String TAG = "A11yActionCompat";

        /**
         * Action that gives input focus to the node.
         * <p>The focus request sends an event of {@link AccessibilityEvent#TYPE_VIEW_FOCUSED}
         * if successful. In the View system, this is handled by {@link View#requestFocus}.
         *
         * <p>The node that is focused should return {@code true} for
         * {@link AccessibilityNodeInfoCompat#isFocused()}.
         *
         * @see #ACTION_ACCESSIBILITY_FOCUS for the difference between system focus and
         * accessibility focus.
         */
        public static final AccessibilityActionCompat ACTION_FOCUS =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_FOCUS, null);

        /**
         * Action that clears input focus of the node.
         * <p>The node that is cleared should return {@code false} for
         * {@link AccessibilityNodeInfoCompat#isFocused()}.
         */
        public static final AccessibilityActionCompat ACTION_CLEAR_FOCUS =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_CLEAR_FOCUS, null);

        /**
         *  Action that selects the node.
         */
        public static final AccessibilityActionCompat ACTION_SELECT =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_SELECT, null);

        /**
         * Action that deselects the node.
         */
        public static final AccessibilityActionCompat ACTION_CLEAR_SELECTION =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_CLEAR_SELECTION, null);

        /**
         * Action that clicks on the node info.
         *
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_CLICKED} event. In the View system,
         * the default handling of this action when performed by a service is to call
         * {@link View#performClick()}, and setting a
         * {@link View#setOnClickListener(View.OnClickListener)} automatically adds this action.
         *
         * <p>{@link #isClickable()} should return true if this action is available.
         */
        public static final AccessibilityActionCompat ACTION_CLICK =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_CLICK, null);

        /**
         * Action that long clicks on the node.
         *
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_LONG_CLICKED} event. In the View system,
         * the default handling of this action when performed by a service is to call
         * {@link View#performLongClick()}, and setting a
         * {@link View#setOnLongClickListener(View.OnLongClickListener)} automatically adds this
         * action.
         *
         * <p>{@link #isLongClickable()} should return true if this action is available.
         */
        public static final AccessibilityActionCompat ACTION_LONG_CLICK =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_LONG_CLICK, null);

        /**
         * Action that gives accessibility focus to the node.
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_ACCESSIBILITY_FOCUSED} event
         * if successful. The node that is focused should return {@code true} for
         * {@link AccessibilityNodeInfoCompat#isAccessibilityFocused()}.
         *
         * <p>This is intended to be used by screen readers to assist with user navigation. Apps
         * changing focus can confuse screen readers, so the resulting behavior can vary by device
         * and screen reader version.
         * <p>This is distinct from {@link #ACTION_FOCUS}, which refers to system focus. System
         * focus is typically used to convey targets for keyboard navigation.
         */
        public static final AccessibilityActionCompat ACTION_ACCESSIBILITY_FOCUS =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null);

        /**
         * Action that clears accessibility focus of the node.
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED} event if successful. The
         * node that is cleared should return {@code false} for
         * {@link AccessibilityNodeInfoCompat#isAccessibilityFocused()}.
         */
        public static final AccessibilityActionCompat ACTION_CLEAR_ACCESSIBILITY_FOCUS =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS, null);

        /**
         * Action that requests to go to the next entity in this node's text
         * at a given movement granularity. For example, move to the next character,
         * word, etc.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT},
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN}<br>
         * <strong>Example:</strong> Move to the previous character and do not extend selection.
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putInt(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
         *           AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER);
         *   arguments.putBoolean(
         *           AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
         *   info.performAction(
         *           AccessibilityActionCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY.getId(),
         *           arguments);
         * </code></pre></p>
         * </p>
         *
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *
         * @see AccessibilityNodeInfoCompat#setMovementGranularities(int)
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         * @see AccessibilityNodeInfoCompat#getMovementGranularities()
         *  AccessibilityNodeInfoCompat.getMovementGranularities()
         *
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_CHARACTER
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_WORD
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_LINE
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_PARAGRAPH
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_PAGE
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PAGE
         */
        public static final AccessibilityActionCompat ACTION_NEXT_AT_MOVEMENT_GRANULARITY =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, null,
                        MoveAtGranularityArguments.class);

        /**
         * Action that requests to go to the previous entity in this node's text
         * at a given movement granularity. For example, move to the next character,
         * word, etc.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT},
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN}<br>
         * <strong>Example:</strong> Move to the next character and do not extend selection.
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putInt(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
         *           AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER);
         *   arguments.putBoolean(
         *           AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
         *   info.performAction(
         *           AccessibilityActionCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY.getId(),
         *           arguments);
         * </code></pre></p>
         * </p>
         *
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
         *
         * @see AccessibilityNodeInfoCompat#setMovementGranularities(int)
         *   AccessibilityNodeInfoCompat.setMovementGranularities(int)
         * @see AccessibilityNodeInfoCompat#getMovementGranularities()
         *  AccessibilityNodeInfoCompat.getMovementGranularities()
         *
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_CHARACTER
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_WORD
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_LINE
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_PARAGRAPH
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH
         * @see AccessibilityNodeInfoCompat#MOVEMENT_GRANULARITY_PAGE
         *  AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PAGE
         */
        public static final AccessibilityActionCompat ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, null,
                        MoveAtGranularityArguments.class);

        /**
         * Action to move to the next HTML element of a given type. For example, move
         * to the BUTTON, INPUT, TABLE, etc.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_HTML_ELEMENT_STRING
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING}<br>
         * <strong>Example:</strong>
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putString(
         *           AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING, "BUTTON");
         *   info.performAction(
         *           AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT.getId(), arguments);
         * </code></pre></p>
         * </p>
         */
        public static final AccessibilityActionCompat ACTION_NEXT_HTML_ELEMENT =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_NEXT_HTML_ELEMENT, null,
                        MoveHtmlArguments.class);

        /**
         * Action to move to the previous HTML element of a given type. For example, move
         * to the BUTTON, INPUT, TABLE, etc.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_HTML_ELEMENT_STRING
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING}<br>
         * <strong>Example:</strong>
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putString(
         *           AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING, "BUTTON");
         *   info.performAction(
         *           AccessibilityActionCompat.ACTION_PREVIOUS_HTML_ELEMENT.getId(), arguments);
         * </code></pre></p>
         * </p>
         */
        public static final AccessibilityActionCompat ACTION_PREVIOUS_HTML_ELEMENT =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_PREVIOUS_HTML_ELEMENT, null,
                        MoveHtmlArguments.class);

        /**
         * Action to scroll the node content forward.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_FORWARD =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_SCROLL_FORWARD, null);

        /**
         * Action to scroll the node content backward.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_BACKWARD =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_SCROLL_BACKWARD, null);

        /**
         * Action to copy the current selection to the clipboard.
         */
        public static final AccessibilityActionCompat ACTION_COPY =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_COPY, null);

        /**
         * Action to paste the current clipboard content.
         */
        public static final AccessibilityActionCompat ACTION_PASTE =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_PASTE, null);

        /**
         * Action to cut the current selection and place it to the clipboard.
         */
        public static final AccessibilityActionCompat ACTION_CUT =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_CUT, null);

        /**
         * Action to set the selection. Performing this action with no arguments
         * clears the selection.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_SELECTION_START_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_START_INT},
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_SELECTION_END_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_END_INT}<br>
         * <strong>Example:</strong>
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putInt(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_START_INT, 1);
         *   arguments.putInt(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_END_INT, 2);
         *   info.performAction(AccessibilityActionCompat.ACTION_SET_SELECTION.getId(), arguments);
         * </code></pre></p>
         * </p>
         *
         * <p> If this is a text selection, the UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_TEXT_SELECTION_CHANGED} event if its selection is
         * updated. This element should also return {@code true} for
         * {@link AccessibilityNodeInfoCompat#isTextSelectable()}.
         *
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_SELECTION_START_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_START_INT
         * @see AccessibilityNodeInfoCompat#ACTION_ARGUMENT_SELECTION_END_INT
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_END_INT
         */
        public static final AccessibilityActionCompat ACTION_SET_SELECTION =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_SET_SELECTION, null,
                        SetSelectionArguments.class);

        /**
         * Action to expand an expandable node.
         */
        public static final AccessibilityActionCompat ACTION_EXPAND =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_EXPAND, null);

        /**
         * Action to collapse an expandable node.
         */
        public static final AccessibilityActionCompat ACTION_COLLAPSE =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_COLLAPSE, null);

        /**
         * Action to dismiss a dismissable node.
         */
        public static final AccessibilityActionCompat ACTION_DISMISS =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfoCompat.ACTION_DISMISS, null);

        /**
         * Action that sets the text of the node. Performing the action without argument,
         * using <code> null</code> or empty {@link CharSequence} will clear the text. This
         * action will also put the cursor at the end of text.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE
         *  AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE}<br>
         * <strong>Example:</strong>
         * <code><pre><p>
         *   Bundle arguments = new Bundle();
         *   arguments.putCharSequence(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE,
         *       "android");
         *  info.performAction(AccessibilityActionCompat.ACTION_SET_TEXT.getId(), arguments);
         * </code></pre></p>
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_TEXT_CHANGED} event if its text is updated.
         * This element should also return {@code true} for
         * {@link AccessibilityNodeInfoCompat#isEditable()}.
         */
        public static final AccessibilityActionCompat ACTION_SET_TEXT =
                new AccessibilityActionCompat(AccessibilityNodeInfoCompat.ACTION_SET_TEXT, null,
                        SetTextArguments.class);

        /**
         * Action that requests the node make its bounding rectangle visible
         * on the screen, scrolling if necessary just enough.
         *
         * @see View#requestRectangleOnScreen(Rect)
         */
        public static final AccessibilityActionCompat ACTION_SHOW_ON_SCREEN =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_ON_SCREEN,
                        android.R.id.accessibilityActionShowOnScreen, null, null, null);

        /**
         * Action that scrolls the node to make the specified collection
         * position visible on screen.
         * <p>
         * <strong>Arguments:</strong>
         * <ul>
         *     <li>{@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_ROW_INT}</li>
         *     <li>{@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_COLUMN_INT}</li>
         * <ul>
         *
         * @see AccessibilityNodeInfoCompat#getCollectionInfo()
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_TO_POSITION =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_TO_POSITION,
                        android.R.id.accessibilityActionScrollToPosition, null, null,
                        ScrollToPositionArguments.class);

        /**
         * Action to scroll the node content up.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_UP =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP,
                        android.R.id.accessibilityActionScrollUp, null, null, null);
        /**
         * Action to scroll the node content left.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_LEFT =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_LEFT,
                        android.R.id.accessibilityActionScrollLeft, null, null, null);

        /**
         * Action to scroll the node content down.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_DOWN =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN,
                        android.R.id.accessibilityActionScrollDown, null, null, null);

        /**
         * Action to scroll the node content right.
         */
        public static final AccessibilityActionCompat ACTION_SCROLL_RIGHT =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_RIGHT,
                        android.R.id.accessibilityActionScrollRight, null, null, null);

        /**
         * Action to move to the page above.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_PAGE_UP =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 29
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_PAGE_UP : null,
                        android.R.id.accessibilityActionPageUp, null, null, null);

        /**
         * Action to move to the page below.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_PAGE_DOWN =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 29
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_PAGE_DOWN : null,
                        android.R.id.accessibilityActionPageDown, null, null, null);

        /**
         * Action to move to the page left.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_PAGE_LEFT =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 29
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_PAGE_LEFT : null,
                        android.R.id.accessibilityActionPageLeft, null, null, null);

        /**
         * Action to move to the page right.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_PAGE_RIGHT =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 29
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_PAGE_RIGHT : null,
                        android.R.id.accessibilityActionPageRight, null, null, null);

        /**
         * Action that context clicks the node.
         *
         * <p>The UI element that implements this should send a
         * {@link AccessibilityEvent#TYPE_VIEW_CONTEXT_CLICKED} event. In the View system,
         * the default handling of this action when performed by a service is to call
         * {@link View#performContextClick()}, and setting a
         * {@link View#setOnContextClickListener(View.OnContextClickListener)} automatically adds
         * this action.
         *
         * <p>A context click usually occurs from a mouse pointer right-click or a stylus button
         * press.
         *
         * <p>{@link #isContextClickable()} should return true if this action is available.
         */
        public static final AccessibilityActionCompat ACTION_CONTEXT_CLICK =
                new AccessibilityActionCompat(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_CONTEXT_CLICK,
                        android.R.id.accessibilityActionContextClick, null, null, null);

        /**
         * Action that sets progress between {@link  RangeInfoCompat#getMin() RangeInfo.getMin()} and
         * {@link  RangeInfoCompat#getMax() RangeInfo.getMax()}. It should use the same value type as
         * {@link RangeInfoCompat#getType() RangeInfo.getType()}
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_PROGRESS_VALUE}
         *
         * @see RangeInfoCompat
         */
        public static final AccessibilityActionCompat ACTION_SET_PROGRESS =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 24
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_PROGRESS : null,
                        android.R.id.accessibilityActionSetProgress, null, null,
                        SetProgressArguments.class);

        /**
         * Action to move a window to a new location.
         * <p>
         * <strong>Arguments:</strong>
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVE_WINDOW_X}
         * {@link AccessibilityNodeInfoCompat#ACTION_ARGUMENT_MOVE_WINDOW_Y}
         */
        public static final AccessibilityActionCompat ACTION_MOVE_WINDOW =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 26
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_MOVE_WINDOW : null,
                        android.R.id.accessibilityActionMoveWindow, null, null,
                        MoveWindowArguments.class);

        /**
         * Action to show a tooltip.
         */
        public static final AccessibilityActionCompat ACTION_SHOW_TOOLTIP =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 28
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_TOOLTIP : null,
                        android.R.id.accessibilityActionShowTooltip, null, null, null);

        /**
         * Action to hide a tooltip. A node should expose this action only for views that are
         * currently showing a tooltip.
         */
        public static final AccessibilityActionCompat ACTION_HIDE_TOOLTIP =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 28
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_HIDE_TOOLTIP : null,
                        android.R.id.accessibilityActionHideTooltip, null, null, null);

        /**
         * Action that presses and holds a node.
         * <p>
         * This action is for nodes that have distinct behavior that depends on how long a press is
         * held. Nodes having a single action for long press should use {@link #ACTION_LONG_CLICK}
         *  instead of this action, and nodes should not expose both actions.
         * <p>
         * When calling {@code performAction(ACTION_PRESS_AND_HOLD, bundle}, use
         * {@link #ACTION_ARGUMENT_PRESS_AND_HOLD_DURATION_MILLIS_INT} to specify how long the
         * node is pressed. The first time an accessibility service performs ACTION_PRES_AND_HOLD
         * on a node, it must specify 0 as ACTION_ARGUMENT_PRESS_AND_HOLD, so the application is
         * notified that the held state has started. To ensure reasonable behavior, the values
         * must be increased incrementally and may not exceed 10,000. UIs requested
         * to hold for times outside of this range should ignore the action.
         * <p>
         * The total time the element is held could be specified by an accessibility user up-front,
         * or may depend on what happens on the UI as the user continues to request the hold.
         * <p>
         *   <strong>Note:</strong> The time between dispatching the action and it arriving in the
         *     UI process is not guaranteed. It is possible on a busy system for the time to expire
         *     unexpectedly. For the case of holding down a key for a repeating action, a delayed
         *     arrival should be benign. Please do not use this sort of action in cases where such
         *     delays will lead to unexpected UI behavior.
         * <p>
         */
        public static final @NonNull AccessibilityActionCompat ACTION_PRESS_AND_HOLD =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 30
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_PRESS_AND_HOLD : null,
                        android.R.id.accessibilityActionPressAndHold, null, null, null);

        /**
         * Action to send an ime actionId which is from
         * {@link android.view.inputmethod.EditorInfo#actionId}. This ime actionId sets by
         * {@link android.widget.TextView#setImeActionLabel(CharSequence, int)}, or it would be
         * {@link android.view.inputmethod.EditorInfo#IME_ACTION_UNSPECIFIED} if no specific
         * actionId has set. A node should expose this action only for views that are currently
         * with input focus and editable.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_IME_ENTER =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 30
                        ? AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER : null,
                        android.R.id.accessibilityActionImeEnter, null, null, null);

        /**
         * Action to start a drag.
         * <p>
         * This action initiates a drag & drop within the system. The source's dragged content is
         * prepared before the drag begins. In View, this action should prepare the arguments to
         * {@link View#startDragAndDrop(ClipData, View.DragShadowBuilder, Object, int)}} and then
         * call the method. The equivalent should be performed for other UI toolkits.
         * </p>
         *
         * @see AccessibilityEventCompat#CONTENT_CHANGE_TYPE_DRAG_STARTED
         */
        public static final @NonNull AccessibilityActionCompat ACTION_DRAG_START =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 32
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_DRAG_START : null,
                        android.R.id.accessibilityActionDragStart, null, null, null);

        /**
         * Action to trigger a drop of the content being dragged.
         * <p>
         * This action is added to potential drop targets if the source started a drag with
         * {@link #ACTION_DRAG_START}. In View, these targets are Views that accepted
         * {@link android.view.DragEvent#ACTION_DRAG_STARTED} and have an
         * {@link View.OnDragListener}.
         * </p>
         *
         * @see AccessibilityEventCompat#CONTENT_CHANGE_TYPE_DRAG_DROPPED
         */
        public static final @NonNull AccessibilityActionCompat ACTION_DRAG_DROP =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 32
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_DRAG_DROP : null,
                        android.R.id.accessibilityActionDragDrop, null, null, null);

        /**
         * Action to cancel a drag.
         * <p>
         * This action is added to the source that started a drag with {@link #ACTION_DRAG_START}.
         * </p>
         *
         * @see AccessibilityEventCompat#CONTENT_CHANGE_TYPE_DRAG_CANCELLED
         */
        public static final @NonNull AccessibilityActionCompat ACTION_DRAG_CANCEL =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 32
                        ?  AccessibilityNodeInfo.AccessibilityAction.ACTION_DRAG_CANCEL : null,
                        android.R.id.accessibilityActionDragCancel, null, null, null);

        /**
         * Action to show suggestions for editable text.
         */
        public static final @NonNull AccessibilityActionCompat ACTION_SHOW_TEXT_SUGGESTIONS =
                new AccessibilityActionCompat(Build.VERSION.SDK_INT >= 33
                        ?   AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_TEXT_SUGGESTIONS
                        :   null, android.R.id.accessibilityActionShowTextSuggestions, null,
                        null, null);

        /**
         * Action that brings fully on screen the next node in the specified direction.
         *
         * <p>This should include wrapping around to the next/previous row, column, etc. in a
         * collection if one is available. If there is no node in that direction, the action should
         * fail and return false.
         *
         * <p>This action should be used instead of {@link
         * AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION} when a widget does not have clear
         * row and column semantics or if a directional search is needed to find a node in a complex
         * ViewGroup where individual nodes may span multiple rows or columns. The implementing
         * widget must send a {@link AccessibilityEventCompat#TYPE_VIEW_TARGETED_BY_SCROLL}
         * accessibility event with the scroll target as the source. An accessibility service can
         * listen for this event, inspect its source, and use the result when determining where to
         * place accessibility focus.
         *
         * <p><strong>Arguments:</strong> {@link #ACTION_ARGUMENT_DIRECTION_INT}. This is a required
         * argument.<br>
         */
        @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
        public static final @NonNull AccessibilityActionCompat ACTION_SCROLL_IN_DIRECTION =
                new AccessibilityActionCompat(
                        Build.VERSION.SDK_INT >= 34 ? Api34Impl.getActionScrollInDirection() : null,
                        android.R.id.accessibilityActionScrollInDirection,
                        null,
                        null,
                        null);

        /**
         * Action to set the extended selection. Performing this action with no arguments clears the
         * selection.
         *
         * <p><strong>Example:</strong> <code><pre><p>
         *  Bundle arguments = new Bundle();
         *  SelectionCompat selection = new SelectionCompat(null, null);
         *  arguments.setParcelable(
         *          AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_PARCELABLE,
         *          selection.mSelection);
         *  info.performAction(
         *          AccessibilityActionCompat.ACTION_SET_EXTENDED_SELECTION.getId(), arguments);
         * </pre></code>
         */
        public static final @NonNull AccessibilityActionCompat ACTION_SET_EXTENDED_SELECTION =
                new AccessibilityActionCompat(
                        isAtLeastB_1() ? Api36MinorImpl.getActionSetExtendedSelection() : null,
                        android.R.id.accessibilityActionSetExtendedSelection,
                        null,
                        null,
                        null);

        final Object mAction;
        private final int mId;
        private final Class<? extends CommandArguments> mViewCommandArgumentClass;

        /**
         */
        @RestrictTo(LIBRARY_GROUP_PREFIX)
        protected final AccessibilityViewCommand mCommand;

        /**
         * Creates a new instance.
         *
         * @param actionId The action id.
         * @param label The action label.
         */
        public AccessibilityActionCompat(int actionId, CharSequence label) {
            this(null, actionId, label, null, null);
        }

        /**
         * Creates a new instance.
         *
         * @param actionId The action id.
         * @param label The action label.
         * @param command The command performed when the service requests the action
         */
        @RestrictTo(LIBRARY_GROUP_PREFIX)
        public AccessibilityActionCompat(int actionId, CharSequence label,
                AccessibilityViewCommand command) {
            this(null, actionId, label, command, null);
        }

        AccessibilityActionCompat(Object action) {
            this(action, 0, null, null, null);
        }

        private AccessibilityActionCompat(int actionId, CharSequence label,
                Class<? extends CommandArguments> viewCommandArgumentClass) {
            this(null, actionId, label, null, viewCommandArgumentClass);
        }

        AccessibilityActionCompat(Object action, int id, CharSequence label,
                AccessibilityViewCommand command,
                Class<? extends CommandArguments> viewCommandArgumentClass) {
            mId = id;
            mCommand = command;
            if (action == null) {
                mAction = new AccessibilityNodeInfo.AccessibilityAction(id, label);
            } else {
                mAction = action;
            }
            mViewCommandArgumentClass = viewCommandArgumentClass;
        }

        /**
         * Gets the id for this action.
         *
         * @return The action id.
         */
        public int getId() {
            return ((AccessibilityNodeInfo.AccessibilityAction) mAction).getId();
        }

        /**
         * Gets the label for this action. Its purpose is to describe the
         * action to user.
         *
         * @return The label.
         */
        public CharSequence getLabel() {
            return ((AccessibilityNodeInfo.AccessibilityAction) mAction).getLabel();
        }

        /**
         * Performs the action.
         * @return If the action was handled.
         * @param view View to act upon.
         * @param arguments Optional action arguments.
         */
        @RestrictTo(LIBRARY_GROUP_PREFIX)
        public boolean perform(View view, Bundle arguments) {
            if (mCommand != null) {
                CommandArguments viewCommandArgument = null;
                if (mViewCommandArgumentClass != null) {
                    try {
                        viewCommandArgument =
                                mViewCommandArgumentClass.getDeclaredConstructor().newInstance();
                        viewCommandArgument.setBundle(arguments);
                    } catch (Exception e) {
                        final String className = mViewCommandArgumentClass == null
                                ? "null" : mViewCommandArgumentClass.getName();
                        Log.e(TAG, "Failed to execute command with argument class "
                                + "ViewCommandArgument: " + className, e);
                    }
                }
                return mCommand.perform(view, viewCommandArgument);
            }
            return false;
        }

        /**
         */
        @RestrictTo(LIBRARY_GROUP_PREFIX)
        public AccessibilityActionCompat createReplacementAction(CharSequence label,
                AccessibilityViewCommand command) {
            return new AccessibilityActionCompat(null, mId, label, command,
                    mViewCommandArgumentClass);
        }

        @Override
        public int hashCode() {
            return mAction != null ? mAction.hashCode() : 0;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (obj == null) {
                return false;
            }
            if (!(obj instanceof AccessibilityNodeInfoCompat.AccessibilityActionCompat)) {
                return false;
            }
            AccessibilityNodeInfoCompat.AccessibilityActionCompat other =
                    (AccessibilityNodeInfoCompat.AccessibilityActionCompat) obj;
            if (mAction == null) {
                if (other.mAction != null) {
                    return false;
                }
            } else if (!mAction.equals(other.mAction)) {
                return false;
            }
            return true;
        }

        @Override
        public @NonNull String toString() {
            StringBuilder builder = new StringBuilder();
            builder.append("AccessibilityActionCompat: ");
            // Mirror AccessibilityNodeInfoCompat.toString's action string.
            String actionName = getActionSymbolicName(mId);
            if (actionName.equals("ACTION_UNKNOWN") && getLabel() != null) {
                actionName = getLabel().toString();
            }
            builder.append(actionName);
            return builder.toString();
        }
    }

    /**
     * Class with information if a node is a collection.
     * <p>
     * A collection of items has rows and columns and may be marked as hierarchical.
     *
     * <p>
     * For example, a list where the items are placed in a vertical layout is a collection with one
     * column and as many rows as the list items. This collection has 3 rows and 1 column and should
     * not be marked as hierarchical since items do not exist at different levels/ranks and there
     * are no nested collections.
     * <ul>
     *     <li>Item 1</li>
     *     <li>Item 2</li>
     *     <li>Item 3</li>
     * </ul>
     *
     * <p>
     * A table is a collection with several rows and several columns. This collection has 2 rows and
     * 3 columns and is not marked as hierarchical:
     *<table>
     *   <tr>
     *     <td>Item 1</td>
     *     <td>Item 2</td>
     *     <td>Item 3</td>
     *   </tr>
     *   <tr>
     *     <td>Item 4</td>
     *     <td>Item 5</td>
     *     <td>Item 6</td>
     *   </tr>
     * </table>
     *
     * <p>
     * Nested collections could be marked as hierarchical. To add outer and inner collections to the
     * same hierarchy, mark them both as hierarchical.
     *
     * <p> For example, if you have a collection with two lists - this collection has an outer
     * list with 3 rows and 1 column and an inner list within "Item 2" with 2 rows and 1 -
     * you can mark both the outer list and the inner list as hierarchical to make them part of
     * the same hierarchy. If a collection does not have any ancestor or descendant hierarchical
     * collections, it does not need to be marked as hierarchical.
     *  <ul>
     *      <li>Item 1</li>
     *      <li> Item 2
     *          <ul>
     *              <li>Item 2A</li>
     *              <li>Item 2B</li>
     *          </ul>
     *      </li>
     *      <li>Item 3</li>
     *  </ul>
     *
     * <p>
     * To be a valid list, a collection has 1 row and any number of columns or 1 column and any
     * number of rows.
     * </p>
     */
    public static class CollectionInfoCompat {
        /** Selection mode where items are not selectable. */
        public static final int SELECTION_MODE_NONE = 0;

        /** Selection mode where a single item may be selected. */
        public static final int SELECTION_MODE_SINGLE = 1;

        /** Selection mode where multiple items may be selected. */
        public static final int SELECTION_MODE_MULTIPLE = 2;

        /**
         * Constant to denote a missing collection count.
         *
         * This should be used for {@code mItemCount} and
         * {@code mImportantForAccessibilityItemCount} when values for those fields are not known.
         */
        public static final int UNDEFINED = AccessibilityNodeInfo.CollectionInfo.UNDEFINED;

        final Object mInfo;

        /**
         * Returns a cached instance if such is available otherwise a new one.
         *
         * @param rowCount The number of rows.
         * @param columnCount The number of columns.
         * @param hierarchical Whether the collection is hierarchical.
         * @param selectionMode The collection's selection mode, one of:
         *            <ul>
         *            <li>{@link #SELECTION_MODE_NONE}
         *            <li>{@link #SELECTION_MODE_SINGLE}
         *            <li>{@link #SELECTION_MODE_MULTIPLE}
         *            </ul>
         *
         * @return An instance.
         */
        public static CollectionInfoCompat obtain(int rowCount, int columnCount,
                boolean hierarchical, int selectionMode) {
            return new CollectionInfoCompat(AccessibilityNodeInfo.CollectionInfo.obtain(
                    rowCount, columnCount, hierarchical, selectionMode));
        }

        /**
         * Returns a cached instance if such is available otherwise a new one.
         *
         * @param rowCount The number of rows, or -1 if count is unknown.
         * @param columnCount The number of columns , or -1 if count is unknown.
         * @param hierarchical Whether the collection is hierarchical.
         *
         * @return An instance.
         */
        public static CollectionInfoCompat obtain(int rowCount, int columnCount,
                boolean hierarchical) {
            return new CollectionInfoCompat(AccessibilityNodeInfo.CollectionInfo.obtain(
                    rowCount, columnCount, hierarchical));
        }

        CollectionInfoCompat(Object info) {
            mInfo = info;
        }

        /**
         * Gets the number of columns.
         *
         * @return The column count, or -1 if count is unknown.
         */
        public int getColumnCount() {
            return ((AccessibilityNodeInfo.CollectionInfo) mInfo).getColumnCount();
        }

        /**
         * Gets the number of rows.
         *
         * @return The row count, or -1 if count is unknown.
         */
        public int getRowCount() {
            return ((AccessibilityNodeInfo.CollectionInfo) mInfo).getRowCount();
        }

        /**
         * Gets if the collection is a hierarchically ordered.
         *
         * @return Whether the collection is hierarchical.
         */
        public boolean isHierarchical() {
            return ((AccessibilityNodeInfo.CollectionInfo) mInfo).isHierarchical();
        }

        /**
         * Gets the collection's selection mode.
         *
         * @return The collection's selection mode, one of:
         *         <ul>
         *         <li>{@link #SELECTION_MODE_NONE}
         *         <li>{@link #SELECTION_MODE_SINGLE}
         *         <li>{@link #SELECTION_MODE_MULTIPLE}
         *         </ul>
         */
        public int getSelectionMode() {
            return ((AccessibilityNodeInfo.CollectionInfo) mInfo).getSelectionMode();
        }

        /**
         * Gets the number of items in the collection.
         *
         * @return The count of items, which may be {@code UNDEFINED} if the count is not known.
         */
        public int getItemCount() {
            if (Build.VERSION.SDK_INT >= 35) {
                return Api35Impl.getItemCount(mInfo);
            }
            return UNDEFINED;
        }

        /**
         * Gets the number of items in the collection considered important for accessibility.
         *
         * @return The count of items important for accessibility, which may be {@code UNDEFINED}
         * if the count is not known.
         */
        public int getImportantForAccessibilityItemCount() {
            if (Build.VERSION.SDK_INT >= 35) {
                return Api35Impl.getImportantForAccessibilityItemCount(mInfo);
            }
            return UNDEFINED;
        }

        /**
         * Class for building {@link CollectionInfoCompat} objects.
         */
        public static final class Builder {
            private int mRowCount = 0;
            private int mColumnCount = 0;
            private boolean mHierarchical = false;
            private int mSelectionMode;
            private int mItemCount = AccessibilityNodeInfo.CollectionInfo.UNDEFINED;
            private int mImportantForAccessibilityItemCount =
                    AccessibilityNodeInfo.CollectionInfo.UNDEFINED;

            /**
             * Creates a new Builder.
             */
            public Builder() {
            }

            /**
             * Sets the row count.
             * @param rowCount The number of rows in the collection.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setRowCount(int rowCount) {
                mRowCount = rowCount;
                return this;
            }

            /**
             * Sets the column count.
             * @param columnCount The number of columns in the collection.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setColumnCount(int columnCount) {
                mColumnCount = columnCount;
                return this;
            }
            /**
             * Sets whether the collection is hierarchical.
             * @param hierarchical Whether the collection is hierarchical.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setHierarchical(boolean hierarchical) {
                mHierarchical = hierarchical;
                return this;
            }

            /**
             * Sets the selection mode.
             * @param selectionMode The selection mode.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setSelectionMode(int selectionMode) {
                mSelectionMode = selectionMode;
                return this;
            }

            /**
             * Sets the number of items in the collection. Can be optionally set for ViewGroups with
             * clear row and column semantics; should be set for all other clients.
             *
             * @param itemCount The number of items in the collection. This should be set to
             *                  {@code UNDEFINED} if the item count is not known.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setItemCount(int itemCount) {
                mItemCount = itemCount;
                return this;
            }

            /**
             * Sets the number of views considered important for accessibility.
             * @param importantForAccessibilityItemCount The number of items important for
             *                                            accessibility.
             * @return This builder.
             */
            public CollectionInfoCompat.@NonNull Builder setImportantForAccessibilityItemCount(
                    int importantForAccessibilityItemCount) {
                mImportantForAccessibilityItemCount = importantForAccessibilityItemCount;
                return this;
            }

            /**
             * Creates a new {@link CollectionInfoCompat} instance.
             */
            public @NonNull CollectionInfoCompat build() {
                if (Build.VERSION.SDK_INT >= 35) {
                    return Api35Impl.buildCollectionInfoCompat(mRowCount, mColumnCount,
                            mHierarchical, mSelectionMode, mItemCount,
                            mImportantForAccessibilityItemCount);
                }

                return CollectionInfoCompat.obtain(mRowCount, mColumnCount, mHierarchical,
                        mSelectionMode);
            }
        }
    }

    /**
     * Class with information if a node is a collection item.
     *
     * <p>A collection item is contained in a collection, it starts at a given row and column in the
     * collection, and spans one or more rows and columns. For example, a header of two related
     * table columns starts at the first row and the first column, spans one row and two columns.
     */
    public static class CollectionItemInfoCompat {
        /**
         * There is no sort direction.
         *
         * @see #getSortDirection()
         * @see Builder#setSortDirection(int)
         */
        public static final int SORT_DIRECTION_NONE = 0;

        /**
         * Items are sorted in ascending order (e.g., A-Z, 0-9).
         *
         * @see #getSortDirection()
         * @see Builder#setSortDirection(int)
         */
        public static final int SORT_DIRECTION_ASCENDING = 1;

        /**
         * Items are sorted in descending order (e.g., Z-A, 9-0).
         *
         * @see #getSortDirection()
         * @see Builder#setSortDirection(int)
         */
        public static final int SORT_DIRECTION_DESCENDING = 2;

        /**
         * Items are sorted, but using a method other than ascending or descending (e.g., based on
         * relevance or a custom algorithm).
         *
         * @see #getSortDirection()
         * @see Builder#setSortDirection(int)
         */
        public static final int SORT_DIRECTION_OTHER = 3;

        @RestrictTo(LIBRARY_GROUP_PREFIX)
        @Retention(RetentionPolicy.SOURCE)
        @IntDef(
                value = {
                    SORT_DIRECTION_NONE,
                    SORT_DIRECTION_ASCENDING,
                    SORT_DIRECTION_DESCENDING,
                    SORT_DIRECTION_OTHER,
                })
        public @interface SortDirection {}

        final Object mInfo;

        /**
         * Returns a cached instance if such is available otherwise a new one.
         *
         * @param rowIndex The row index at which the item is located.
         * @param rowSpan The number of rows the item spans.
         * @param columnIndex The column index at which the item is located.
         * @param columnSpan The number of columns the item spans.
         * @param heading Whether the item is a heading. This should be set to false and the newer
         *                {@link AccessibilityNodeInfoCompat#setHeading(boolean)} used to identify
         *                headings.
         * @param selected Whether the item is selected.
         * @return An instance.
         */
        public static CollectionItemInfoCompat obtain(int rowIndex, int rowSpan,
                int columnIndex, int columnSpan, boolean heading, boolean selected) {
            return new CollectionItemInfoCompat(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                    rowIndex, rowSpan, columnIndex, columnSpan, heading, selected));
        }

        /**
         * Returns a cached instance if such is available otherwise a new one.
         *
         * @param rowIndex The row index at which the item is located.
         * @param rowSpan The number of rows the item spans.
         * @param columnIndex The column index at which the item is located.
         * @param columnSpan The number of columns the item spans.
         * @param heading Whether the item is a heading. This should be set to false and the newer
         *                {@link AccessibilityNodeInfoCompat#setHeading(boolean)} used to identify
         *                headings.
         * @return An instance.
         */
        public static CollectionItemInfoCompat obtain(int rowIndex, int rowSpan,
                int columnIndex, int columnSpan, boolean heading) {
            return new CollectionItemInfoCompat(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                    rowIndex, rowSpan, columnIndex, columnSpan, heading));
        }

        CollectionItemInfoCompat(Object info) {
            mInfo = info;
        }

        /**
         * Gets the column index at which the item is located.
         *
         * @return The column index.
         */
        public int getColumnIndex() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).getColumnIndex();
        }

        /**
         * Gets the number of columns the item spans.
         *
         * @return The column span.
         */
        public int getColumnSpan() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).getColumnSpan();
        }

        /**
         * Gets the row index at which the item is located.
         *
         * @return The row index.
         */
        public int getRowIndex() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).getRowIndex();
        }

        /**
         * Gets the number of rows the item spans.
         *
         * @return The row span.
         */
        public int getRowSpan() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).getRowSpan();
        }

        /**
         * Gets if the collection item is a heading. For example, section
         * heading, table header, etc.
         *
         * @return If the item is a heading.
         * @deprecated Use {@link AccessibilityNodeInfoCompat#isHeading()}
         */
        @SuppressWarnings("deprecation")
        @Deprecated
        public boolean isHeading() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).isHeading();
        }

        /**
         * Gets if the collection item is selected.
         *
         * @return If the item is selected.
         */
        public boolean isSelected() {
            return ((AccessibilityNodeInfo.CollectionItemInfo) mInfo).isSelected();
        }

        /**
         * Gets the row title at which the item is located.
         *
         * @return The row title.
         */
        public @Nullable String getRowTitle() {
            if (Build.VERSION.SDK_INT >= 33) {
                return Api33Impl.getCollectionItemRowTitle(mInfo);
            } else {
                return null;
            }
        }

        /**
         * Gets the column title at which the item is located.
         *
         * @return The column title.
         */
        public @Nullable String getColumnTitle() {
            if (Build.VERSION.SDK_INT >= 33) {
                return Api33Impl.getCollectionItemColumnTitle(mInfo);
            } else {
                return null;
            }
        }

        /**
         * Gets the sort direction applied to the data associated with this node.
         *
         * <p>This item can only be set on a heading node within a table collection. Given the
         * heading node's collection item, a subsequent collection item uses this sort direction if
         * it has the same row or column index, and a greater index in the other dimension. For
         * example, an item at row 2, column 2 can reference a heading at row 2, column 1 for its
         * sort direction.
         *
         * <p>Compatibility:
         *
         * <ul>
         *   <li>API &lt; 36.1: Always returns SORT_DIRECTION_NONE
         * </ul>
         *
         * @return The current sort direction.
         */
        public @SortDirection int getSortDirection() {
            if (isAtLeastB_1()) {
                return Api36MinorImpl.getCollectionItemSortDirection(mInfo);
            }

            return SORT_DIRECTION_NONE;
        }

        /** Builder for creating {@link CollectionItemInfoCompat} objects. */
        public static final class Builder {
            private boolean mHeading;
            private int mColumnIndex;
            private int mRowIndex;
            private int mColumnSpan;
            private int mRowSpan;
            private boolean mSelected;
            private String mRowTitle;
            private String mColumnTitle;
            private int mSortDirection;

            /** Creates a new Builder. */
            public Builder() {}

            /**
             * Sets the collection item is a heading.
             *
             * @param heading The heading state
             * @return This builder
             */
            public @NonNull Builder setHeading(boolean heading) {
                mHeading = heading;
                return this;
            }

            /**
             * Sets the column index at which the item is located.
             *
             * @param columnIndex The column index
             * @return This builder
             */
            public @NonNull Builder setColumnIndex(int columnIndex) {
                mColumnIndex = columnIndex;
                return this;
            }

            /**
             * Sets the row index at which the item is located.
             *
             * @param rowIndex The row index
             * @return This builder
             */
            public @NonNull Builder setRowIndex(int rowIndex) {
                mRowIndex = rowIndex;
                return this;
            }

            /**
             * Sets the number of columns the item spans.
             *
             * @param columnSpan The number of columns spans
             * @return This builder
             */
            public @NonNull Builder setColumnSpan(int columnSpan) {
                mColumnSpan = columnSpan;
                return this;
            }

            /**
             * Sets the number of rows the item spans.
             *
             * @param rowSpan The number of rows spans
             * @return This builder
             */
            public @NonNull Builder setRowSpan(int rowSpan) {
                mRowSpan = rowSpan;
                return this;
            }

            /**
             * Sets the collection item is selected.
             *
             * @param selected The number of rows spans
             * @return This builder
             */
            public @NonNull Builder setSelected(boolean selected) {
                mSelected = selected;
                return this;
            }

            /**
             * Sets the row title at which the item is located.
             *
             * @param rowTitle The row title
             * @return This builder
             */
            public @NonNull Builder setRowTitle(@Nullable String rowTitle) {
                mRowTitle = rowTitle;
                return this;
            }

            /**
             * Sets the column title at which the item is located.
             *
             * @param columnTitle The column title
             * @return This builder
             */
            public @NonNull Builder setColumnTitle(@Nullable String columnTitle) {
                mColumnTitle = columnTitle;
                return this;
            }

            /**
             * Sets the sort direction for this item.
             *
             * <p>Valid only if {@link AccessibilityNodeInfo#isHeading()} returns {@code true}.
             * Indicates that collection content associated with this heading is presented in the
             * indicated sort direction. It should only be called by accessibility providers. For
             * accessibility services, see {@link #getSortDirection()} to query the current state.
             *
             * @param sortDirection the sort direction of this collection item info
             * @return This builder
             */
            @NonNull
            public Builder setSortDirection(@SortDirection int sortDirection) {
                mSortDirection = sortDirection;
                return this;
            }

            /** Builds and returns a {@link AccessibilityNodeInfo.CollectionItemInfo}. */
            public @NonNull CollectionItemInfoCompat build() {
                if (isAtLeastB_1()) {
                    return Api36MinorImpl.buildCollectionItemInfoCompat(
                            mHeading,
                            mColumnIndex,
                            mRowIndex,
                            mColumnSpan,
                            mRowSpan,
                            mSelected,
                            mRowTitle,
                            mColumnTitle,
                            mSortDirection);
                } else if (Build.VERSION.SDK_INT >= 33) {
                    return Api33Impl.buildCollectionItemInfoCompat(
                            mHeading,
                            mColumnIndex,
                            mRowIndex,
                            mColumnSpan,
                            mRowSpan,
                            mSelected,
                            mRowTitle,
                            mColumnTitle);
                } else {
                    return new CollectionItemInfoCompat(
                            AccessibilityNodeInfo.CollectionItemInfo.obtain(
                                    mRowIndex,
                                    mRowSpan,
                                    mColumnIndex,
                                    mColumnSpan,
                                    mHeading,
                                    mSelected));
                }
            }
        }
    }

    /**
     * Class with information if a node is a range.
     */
    public static class RangeInfoCompat {
        /** Range type: integer. */
        public static final int RANGE_TYPE_INT = 0;
        /** Range type: float. */
        public static final int RANGE_TYPE_FLOAT = 1;
        /** Range type: percent with values from zero to one.*/
        public static final int RANGE_TYPE_PERCENT = 2;

        /**
         * Obtains a cached instance if such is available otherwise a new one.
         *
         * @param type The type of the range.
         * @param min The min value.
         * @param max The max value.
         * @param current The current value.
         * @return The instance
         */
        public static RangeInfoCompat obtain(int type, float min, float max, float current) {
            return new RangeInfoCompat(
                    AccessibilityNodeInfo.RangeInfo.obtain(type, min, max, current));
        }

        final Object mInfo;

        RangeInfoCompat(Object info) {
            mInfo = info;
        }

        /**
         * Creates a new range.
         *
         * @param type The type of the range.
         * @param min The minimum value. Use {@code Float.NEGATIVE_INFINITY} if the range has no
         *            minimum.
         * @param max The maximum value. Use {@code Float.POSITIVE_INFINITY} if the range has no
         *            maximum.
         * @param current The current value.
         */
        public RangeInfoCompat(int type, float min, float max, float current) {
            if (Build.VERSION.SDK_INT >= 30) {
                mInfo = Api30Impl.createRangeInfo(type, min, max, current);
            } else {
                mInfo = AccessibilityNodeInfo.RangeInfo.obtain(type, min, max, current);
            }
        }

        /**
         * Gets the current value.
         *
         * @return The current value.
         */
        public float getCurrent() {
            return ((AccessibilityNodeInfo.RangeInfo) mInfo).getCurrent();
        }

        /**
         * Gets the max value.
         *
         * @return The max value.
         */
        public float getMax() {
            return ((AccessibilityNodeInfo.RangeInfo) mInfo).getMax();
        }

        /**
         * Gets the min value.
         *
         * @return The min value.
         */
        public float getMin() {
            return ((AccessibilityNodeInfo.RangeInfo) mInfo).getMin();
        }

        /**
         * Gets the range type.
         *
         * @return The range type.
         *
         * @see #RANGE_TYPE_INT
         * @see #RANGE_TYPE_FLOAT
         * @see #RANGE_TYPE_PERCENT
         */
        public int getType() {
            return ((AccessibilityNodeInfo.RangeInfo) mInfo).getType();
        }
    }

    /**
     * Class with information of touch delegated views and regions.
     */
    public static final class TouchDelegateInfoCompat {
        final TouchDelegateInfo mInfo;

        /**
         * Create a new instance of {@link TouchDelegateInfoCompat}.
         *
         * @param targetMap A map from regions (in view coordinates) to delegated views.
         */
        public TouchDelegateInfoCompat(@NonNull Map<Region, View> targetMap) {
            if (Build.VERSION.SDK_INT >= 29) {
                mInfo = new TouchDelegateInfo(targetMap);
            } else {
                mInfo = null;
            }
        }

        TouchDelegateInfoCompat(@NonNull TouchDelegateInfo info) {
            mInfo = info;
        }

        /**
         * Returns the number of touch delegate target region.
         * <p>
         * Compatibility:
         * <ul>
         *     <li>API &lt; 29: Always returns {@code 0}</li>
         * </ul>
         *
         * @return Number of touch delegate target region.
         */
        public @IntRange(from = 0) int getRegionCount() {
            if (Build.VERSION.SDK_INT >= 29) {
                return mInfo.getRegionCount();
            }
            return 0;
        }

        /**
         * Return the {@link Region} at the given index.
         * <p>
         * Compatibility:
         * <ul>
         *     <li>API &lt; 29: Always returns {@code null}</li>
         * </ul>
         *
         * @param index The desired index, must be between 0 and {@link #getRegionCount()}-1.
         * @return Returns the {@link Region} stored at the given index.
         */
        public @Nullable Region getRegionAt(@IntRange(from = 0) int index) {
            if (Build.VERSION.SDK_INT >= 29) {
                return mInfo.getRegionAt(index);
            }
            return null;
        }

        /**
         * Return the target {@link AccessibilityNodeInfoCompat} for the given {@link Region}.
         * <p>
         *   <strong>Note:</strong> This api can only be called from
         *   {@link android.accessibilityservice.AccessibilityService}.
         * </p>
         * <p>
         * Compatibility:
         * <ul>
         *     <li>API &lt; 29: Always returns {@code null}</li>
         * </ul>
         *
         * @param region The region retrieved from {@link #getRegionAt(int)}.
         * @return The target node associates with the given region.
         */
        public @Nullable AccessibilityNodeInfoCompat getTargetForRegion(@NonNull Region region) {
            if (Build.VERSION.SDK_INT >= 29) {
                AccessibilityNodeInfo info = mInfo.getTargetForRegion(region);
                if (info != null) {
                    return AccessibilityNodeInfoCompat.wrap(info);
                }
            }
            return null;
        }
    }

    /**
     * Compat class for AccessibilityNodeInfo.SelectionPosition, which is a class that defines
     * either the start or end of a selection that can span across multiple AccessibilityNodeInfo
     * objects.
     *
     * @see AccessibilityNodeInfo.SelectionPosition
     *     <p>Compatibility:
     *     <ul>
     *       <li>API &lt: 36.1: Class methods perform no-op behavior.
     *     </ul>
     */
    public static final class SelectionPositionCompat {
        final SelectionPosition mPosition;

        /**
         * Instantiates a new SelectionPositionCompat.
         *
         * @param node The {@link AccessibilityNodeInfoCompat} for the node of this selection
         *     position.
         * @param offset The offset for a {@link SelectionPositionCompat} within {@code view}'s text
         *     content, which should be a value between 0 and the length of {@code view}'s text.
         */
        public SelectionPositionCompat(@NonNull AccessibilityNodeInfoCompat node, int offset) {
            if (isAtLeastB_1()) {
                mPosition = new SelectionPosition(node.unwrap(), offset);
            } else {
                mPosition = null;
            }
        }

        /**
         * Instantiates a new SelectionPositionCompat.
         *
         * @param view The {@link View} containing the text associated with this selection position.
         * @param offset The offset for a selection position within {@code view}'s text content,
         *     which should be a value between 0 and the length of {@code view}'s text.
         */
        public SelectionPositionCompat(@NonNull View view, int offset) {
            if (isAtLeastB_1()) {
                mPosition = new SelectionPosition(view, offset);
            } else {
                mPosition = null;
            }
        }

        /**
         * Instantiates a new SelectionPositionCompat.
         *
         * @param view The view whose virtual descendant is associated with the selection position.
         * @param virtualDescendantId The ID of the virtual descendant within {@code view}'s virtual
         *     subtree that contains the selection position.
         * @param offset The offset for a selection position within the virtual descendant's text
         *     content, which should be a value between 0 and the length of the descendant's text.
         */
        public SelectionPositionCompat(@NonNull View view, int virtualDescendantId, int offset) {
            if (isAtLeastB_1()) {
                mPosition = new SelectionPosition(view, virtualDescendantId, offset);
            } else {
                mPosition = null;
            }
        }

        /**
         * Instantiates a new SelectionPositionCompat.
         *
         * @param position The underlying SelectionPosition to wrap.
         */
        public SelectionPositionCompat(@NonNull SelectionPosition position) {
            if (isAtLeastB_1()) {
                mPosition = position;
            } else {
                mPosition = null;
            }
        }

        /**
         * @return The node associated with {@code this} {@link SelectionPositionCompat}
         *     <p>Compatibility:
         *     <ul>
         *       <li>API &lt: 36.1: Always returns {@code null}
         *     </ul>
         */
        public @Nullable AccessibilityNodeInfoCompat getNode() {
            if (isAtLeastB_1()) {
                return AccessibilityNodeInfoCompat.wrap(mPosition.getNode());
            } else {
                return null;
            }
        }

        /**
         * @return A value from 0 to the length of {@link #getNode()}'s content representing the
         *     offset of the {@link SelectionPositionCompat}
         *     <p>Compatibility:
         *     <ul>
         *       <li>API &lt: 36.1: Always returns {@code -1}
         *     </ul>
         */
        public int getOffset() {
            if (isAtLeastB_1()) {
                return mPosition.getOffset();
            } else {
                return -1;
            }
        }

        /**
         * Compatibility:
         *
         * <ul>
         *   <li>API &lt: 36.1: Always returns {@code 0}
         * </ul>
         */
        @Override
        public int hashCode() {
            if (isAtLeastB_1()) {
                return mPosition != null ? mPosition.hashCode() : 0;
            } else {
                return 0;
            }
        }

        /**
         * Compatibility:
         *
         * <ul>
         *   <li>API &lt: 36.1: Always returns {@code false}
         * </ul>
         */
        @Override
        public boolean equals(Object other) {
            if (isAtLeastB_1()) {
                return mPosition != null ? mPosition.equals(other) : false;
            } else {
                return false;
            }
        }
    }

    /**
     * Compat class for AccessibilityNodeInfo.Selection, which is a class that represents a
     * selection of content that may extend across more than one {@link AccessibilityNodeInfo}
     * instance.
     *
     * @see AccessibilityNodeInfo.Selection
     *     <p>Compatibility:
     *     <ul>
     *       <li>API &lt: 36.1: Class methods perform no-op behavior.
     *     </ul>
     */
    public static final class SelectionCompat {
        final Selection mSelection;

        /**
         * Instantiates a new SelectionCompat.
         *
         * @param start The start of the extended selection.
         * @param end The end of the extended selection.
         */
        public SelectionCompat(
                @NonNull SelectionPositionCompat start, @NonNull SelectionPositionCompat end) {
            if (isAtLeastB_1()) {
                mSelection = new Selection(start.mPosition, end.mPosition);
            } else {
                mSelection = null;
            }
        }

        /**
         * Instantiates a new SelectionCompat.
         *
         * @param selection The underlying Selection to wrap.
         */
        public SelectionCompat(@Nullable Selection selection) {
            if (isAtLeastB_1()) {
                mSelection = selection;
            } else {
                mSelection = null;
            }
        }

        /**
         * @return The start of the extended selection.
         *     <p>Compatibility:
         *     <ul>
         *       <li>API &lt: 36.1: Always returns {@code null}
         *     </ul>
         */
        public @Nullable SelectionPositionCompat getStart() {
            if (isAtLeastB_1()) {
                return new SelectionPositionCompat(mSelection.getStart());
            } else {
                return null;
            }
        }

        /**
         * @return The end of the extended selection.
         *     <p>Compatibility:
         *     <ul>
         *       <li>API &lt: 36.1: Always returns {@code null}
         *     </ul>
         */
        public @Nullable SelectionPositionCompat getEnd() {
            if (isAtLeastB_1()) {
                return new SelectionPositionCompat(mSelection.getEnd());
            } else {
                return null;
            }
        }

        /**
         * Compatibility:
         *
         * <ul>
         *   <li>API &lt: 36.1: Always returns {@code 0}
         * </ul>
         */
        @Override
        public int hashCode() {
            if (isAtLeastB_1()) {
                return mSelection != null ? mSelection.hashCode() : 0;
            } else {
                return 0;
            }
        }

        /**
         * Compatibility:
         *
         * <ul>
         *   <li>API &lt: 36.1: Always returns {@code false}
         * </ul>
         */
        @Override
        public boolean equals(Object obj) {
            if (isAtLeastB_1()) {
                return mSelection != null ? mSelection.equals(obj) : false;
            } else {
                return false;
            }
        }
    }

    private static final String ROLE_DESCRIPTION_KEY = "AccessibilityNodeInfo.roleDescription";

    private static final String PANE_TITLE_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.PANE_TITLE_KEY";

    private static final String TOOLTIP_TEXT_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.TOOLTIP_TEXT_KEY";

    private static final String HINT_TEXT_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.HINT_TEXT_KEY";

    private static final String BOOLEAN_PROPERTY_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.BOOLEAN_PROPERTY_KEY";

    private static final String SPANS_ID_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SPANS_ID_KEY";

    private static final String SPANS_START_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SPANS_START_KEY";

    private static final String SPANS_END_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SPANS_END_KEY";

    private static final String SPANS_FLAGS_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SPANS_FLAGS_KEY";

    private static final String SPANS_ACTION_ID_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SPANS_ACTION_ID_KEY";

    private static final String STATE_DESCRIPTION_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.STATE_DESCRIPTION_KEY";

    private static final String SUPPLEMENTAL_DESCRIPTION_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SUPPLEMENTAL_DESCRIPTION_KEY";

    private static final String UNIQUE_ID_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.UNIQUE_ID_KEY";

    private static final String CONTAINER_TITLE_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.CONTAINER_TITLE_KEY";

    private static final String BOUNDS_IN_WINDOW_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.BOUNDS_IN_WINDOW_KEY";

    private static final String EXPANDED_STATE_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.EXPANDED_STATE_KEY";

    private static final String MIN_DURATION_BETWEEN_CONTENT_CHANGES_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat."
                    + "MIN_DURATION_BETWEEN_CONTENT_CHANGES_KEY";
    private static final String IS_REQUIRED_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.IS_REQUIRED_KEY";

    private static final String CHECKED_KEY =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.CHECKED_KEY";

    // These don't line up with the internal framework constants, since they are independent
    // and we might as well get all 32 bits of utility here.
    private static final int BOOLEAN_PROPERTY_SCREEN_READER_FOCUSABLE = 0x00000001;
    private static final int BOOLEAN_PROPERTY_IS_HEADING = 0x00000002;
    private static final int BOOLEAN_PROPERTY_IS_SHOWING_HINT = 0x00000004;
    private static final int BOOLEAN_PROPERTY_IS_TEXT_ENTRY_KEY = 0x00000008;

    private static final int BOOLEAN_PROPERTY_HAS_REQUEST_INITIAL_ACCESSIBILITY_FOCUS = 1 << 5;
    private static final int BOOLEAN_PROPERTY_ACCESSIBILITY_DATA_SENSITIVE = 1 << 6;
    private static final int BOOLEAN_PROPERTY_TEXT_SELECTABLE = 1 << 23;
    private static final int BOOLEAN_PROPERTY_SUPPORTS_GRANULAR_SCROLLING = 1 << 26;

    private final AccessibilityNodeInfo mInfo;

    /**
     *  androidx.customview.widget.ExploreByTouchHelper.HOST_ID = -1;
     */
    @RestrictTo(LIBRARY_GROUP_PREFIX)
    public int mParentVirtualDescendantId = NO_ID;

    private int mVirtualDescendantId = NO_ID;

    // Actions introduced in IceCreamSandwich

    /**
     * Action that focuses the node.
     * @see AccessibilityActionCompat#ACTION_FOCUS
     */
    public static final int ACTION_FOCUS = 0x00000001;

    /**
     * Action that unfocuses the node.
     * @see AccessibilityActionCompat#ACTION_CLEAR_FOCUS
     */
    public static final int ACTION_CLEAR_FOCUS = 0x00000002;

    /**
     * Action that selects the node.
     * @see AccessibilityActionCompat#ACTION_SELECT
     */
    public static final int ACTION_SELECT = 0x00000004;

    /**
     * Action that unselects the node.
     * @see AccessibilityActionCompat#ACTION_CLEAR_SELECTION
     */
    public static final int ACTION_CLEAR_SELECTION = 0x00000008;

    /**
     * Action that clicks on the node info.
     * @see AccessibilityActionCompat#ACTION_CLICK
     */
    public static final int ACTION_CLICK = 0x00000010;

    /**
     * Action that long clicks on the node.
     * @see AccessibilityActionCompat#ACTION_LONG_CLICK
     */
    public static final int ACTION_LONG_CLICK = 0x00000020;

    // Actions introduced in JellyBean

    /**
     * Action that gives accessibility focus to the node.
     * @see AccessibilityActionCompat#ACTION_ACCESSIBILITY_FOCUS
     */
    public static final int ACTION_ACCESSIBILITY_FOCUS = 0x00000040;

    /**
     * Action that clears accessibility focus of the node.
     */
    public static final int ACTION_CLEAR_ACCESSIBILITY_FOCUS = 0x00000080;

    /**
     * Action that requests to go to the next entity in this node's text
     * at a given movement granularity. For example, move to the next character,
     * word, etc.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT}<,
     * {@link #ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN}<br>
     * <strong>Example:</strong> Move to the previous character and do not extend selection.
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
     *           AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
     *   arguments.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN,
     *           false);
     *   info.performAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, arguments);
     * </code></pre></p>
     * </p>
     *
     * @see #ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
     * @see #ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
     *
     * @see #setMovementGranularities(int)
     * @see #getMovementGranularities()
     *
     * @see #MOVEMENT_GRANULARITY_CHARACTER
     * @see #MOVEMENT_GRANULARITY_WORD
     * @see #MOVEMENT_GRANULARITY_LINE
     * @see #MOVEMENT_GRANULARITY_PARAGRAPH
     * @see #MOVEMENT_GRANULARITY_PAGE
     * @see AccessibilityActionCompat#ACTION_NEXT_AT_MOVEMENT_GRANULARITY
     */
    public static final int ACTION_NEXT_AT_MOVEMENT_GRANULARITY = 0x00000100;

    /**
     * Action that requests to go to the previous entity in this node's text
     * at a given movement granularity. For example, move to the next character,
     * word, etc.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT}<,
     * {@link #ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN}<br>
     * <strong>Example:</strong> Move to the next character and do not extend selection.
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
     *           AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
     *   arguments.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN,
     *           false);
     *   info.performAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
     *           arguments);
     * </code></pre></p>
     * </p>
     *
     * @see #ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT
     * @see #ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN
     *
     * @see #setMovementGranularities(int)
     * @see #getMovementGranularities()
     *
     * @see #MOVEMENT_GRANULARITY_CHARACTER
     * @see #MOVEMENT_GRANULARITY_WORD
     * @see #MOVEMENT_GRANULARITY_LINE
     * @see #MOVEMENT_GRANULARITY_PARAGRAPH
     * @see #MOVEMENT_GRANULARITY_PAGE
     * @see AccessibilityActionCompat#ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY
     */
    public static final int ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY = 0x00000200;

    /**
     * Action to move to the next HTML element of a given type. For example, move
     * to the BUTTON, INPUT, TABLE, etc.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_HTML_ELEMENT_STRING}<br>
     * <strong>Example:</strong>
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING, "BUTTON");
     *   info.performAction(AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, arguments);
     * </code></pre></p>
     * </p>
     * @see AccessibilityActionCompat#ACTION_NEXT_HTML_ELEMENT
     */
    public static final int ACTION_NEXT_HTML_ELEMENT = 0x00000400;

    /**
     * Action to move to the previous HTML element of a given type. For example, move
     * to the BUTTON, INPUT, TABLE, etc.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_HTML_ELEMENT_STRING}<br>
     * <strong>Example:</strong>
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putString(AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING, "BUTTON");
     *   info.performAction(AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT, arguments);
     * </code></pre></p>
     * </p>
     *
     * @see AccessibilityActionCompat#ACTION_PREVIOUS_HTML_ELEMENT
     */
    public static final int ACTION_PREVIOUS_HTML_ELEMENT = 0x00000800;

    /**
     * Action to scroll the node content forward.
     * @see AccessibilityActionCompat#ACTION_SCROLL_FORWARD
     */
    public static final int ACTION_SCROLL_FORWARD = 0x00001000;

    /**
     * Action to scroll the node content backward.
     * @see AccessibilityActionCompat#ACTION_SCROLL_BACKWARD
     */
    public static final int ACTION_SCROLL_BACKWARD = 0x00002000;

    // Actions introduced in JellyBeanMr2

    /**
     * Action to copy the current selection to the clipboard.
     * @see AccessibilityActionCompat#ACTION_COPY
     */
    public static final int ACTION_COPY = 0x00004000;

    /**
     * Action to paste the current clipboard content.
     * @see AccessibilityActionCompat#ACTION_PASTE
     */
    public static final int ACTION_PASTE = 0x00008000;

    /**
     * Action to cut the current selection and place it to the clipboard.
     * @see AccessibilityActionCompat#ACTION_CUT
     */
    public static final int ACTION_CUT = 0x00010000;

    /**
     * Action to set the selection. Performing this action with no arguments
     * clears the selection.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_SELECTION_START_INT},
     * {@link #ACTION_ARGUMENT_SELECTION_END_INT}<br>
     * <strong>Example:</strong>
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, 1);
     *   arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, 2);
     *   info.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, arguments);
     * </code></pre></p>
     * </p>
     *
     * @see #ACTION_ARGUMENT_SELECTION_START_INT
     * @see #ACTION_ARGUMENT_SELECTION_END_INT
     * @see AccessibilityActionCompat#ACTION_SET_SELECTION
     */
    public static final int ACTION_SET_SELECTION = 0x00020000;

    /**
     * Action to expand an expandable node.
     * @see AccessibilityActionCompat#ACTION_EXPAND
     */
    public static final int ACTION_EXPAND = 0x00040000;

    /**
     * Action to collapse an expandable node.
     * @see AccessibilityActionCompat#ACTION_COLLAPSE
     */
    public static final int ACTION_COLLAPSE = 0x00080000;

    /**
     * Action to dismiss a dismissible node.
     * @see AccessibilityActionCompat#ACTION_DISMISS
     */
    public static final int ACTION_DISMISS = 0x00100000;

    /**
     * Action that sets the text of the node. Performing the action without argument, using <code>
     * null</code> or empty {@link CharSequence} will clear the text. This action will also put the
     * cursor at the end of text.
     * <p>
     * <strong>Arguments:</strong> {@link #ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE}<br>
     * <strong>Example:</strong>
     * <code><pre><p>
     *   Bundle arguments = new Bundle();
     *   arguments.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE,
     *       "android");
     *   info.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, arguments);
     * </code></pre></p>
     * @see AccessibilityActionCompat#ACTION_SET_TEXT
     */
    public static final int ACTION_SET_TEXT = 0x00200000;

    // Action arguments

    /**
     * Argument for which movement granularity to be used when traversing the node text.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong> {@link #ACTION_NEXT_AT_MOVEMENT_GRANULARITY},
     * {@link #ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY}
     * </p>
     */
    public static final String ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT =
        "ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT";

    /**
     * Argument for which HTML element to get moving to the next/previous HTML element.
     *
     * <p><strong>Type:</strong> String<br>
     * <strong>Actions:</strong> {@link AccessibilityActionCompat#ACTION_NEXT_HTML_ELEMENT}, {@link
     * AccessibilityActionCompat#ACTION_PREVIOUS_HTML_ELEMENT}
     */
    public static final String ACTION_ARGUMENT_HTML_ELEMENT_STRING =
            "ACTION_ARGUMENT_HTML_ELEMENT_STRING";

    /**
     * Argument for specifying the extended selection.
     *
     * <p><strong>Type:</strong> {@link AccessibilityNodeInfoCompat.SelectionCompat}<br>
     * <strong>Actions:</strong>
     *
     * <ul>
     *   <li>{@link AccessibilityActionCompat#ACTION_SET_EXTENDED_SELECTION}
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_SET_EXTENDED_SELECTION
     */
    public static final String ACTION_ARGUMENT_SELECTION_PARCELABLE =
            "androidx.core.view.accessibility.action.ARGUMENT_SELECTION_PARCELABLE";

    /**
     * Argument for whether when moving at granularity to extend the selection or to move it
     * otherwise.
     *
     * <p><strong>Type:</strong> boolean<br>
     * <strong>Actions:</strong> {@link #ACTION_NEXT_AT_MOVEMENT_GRANULARITY}, {@link
     * #ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY}
     *
     * @see AccessibilityActionCompat#ACTION_NEXT_AT_MOVEMENT_GRANULARITY
     * @see AccessibilityActionCompat#ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY
     */
    public static final String ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN =
            "ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN";

    /**
     * Argument for specifying the selection start.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong> {@link #ACTION_SET_SELECTION}
     * </p>
     *
     * @see AccessibilityActionCompat#ACTION_SET_SELECTION
     */
    public static final String ACTION_ARGUMENT_SELECTION_START_INT =
            "ACTION_ARGUMENT_SELECTION_START_INT";

    /**
     * Argument for specifying the selection end.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong> {@link #ACTION_SET_SELECTION}
     * </p>
     *
     * @see AccessibilityActionCompat#ACTION_SET_SELECTION
     */
    public static final String ACTION_ARGUMENT_SELECTION_END_INT =
            "ACTION_ARGUMENT_SELECTION_END_INT";

    /**
     * Argument for specifying the text content to set
     * <p>
     * <strong>Type:</strong> CharSequence<br>
     * <strong>Actions:</strong> {@link #ACTION_SET_TEXT}
     * </p>
     *
     * @see AccessibilityActionCompat#ACTION_SET_TEXT
     */
    public static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
            "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

    /**
     * Argument for specifying the collection row to make visible on screen.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION
     */
    public static final String ACTION_ARGUMENT_ROW_INT =
            "android.view.accessibility.action.ARGUMENT_ROW_INT";

    /**
     * Argument for specifying the collection column to make visible on screen.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION
     */
    public static final String ACTION_ARGUMENT_COLUMN_INT =
            "android.view.accessibility.action.ARGUMENT_COLUMN_INT";

    /**
     * Argument for specifying the progress value to set.
     * <p>
     * <strong>Type:</strong> float<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_SET_PROGRESS}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_SET_PROGRESS
     */
    public static final String ACTION_ARGUMENT_PROGRESS_VALUE =
            "android.view.accessibility.action.ARGUMENT_PROGRESS_VALUE";

    /**
     * Argument for specifying the x coordinate to which to move a window.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_MOVE_WINDOW}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_MOVE_WINDOW
     */
    public static final String ACTION_ARGUMENT_MOVE_WINDOW_X =
            "ACTION_ARGUMENT_MOVE_WINDOW_X";

    /**
     * Argument for specifying the y coordinate to which to move a window.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_MOVE_WINDOW}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_MOVE_WINDOW
     */
    public static final String ACTION_ARGUMENT_MOVE_WINDOW_Y =
            "ACTION_ARGUMENT_MOVE_WINDOW_Y";

    /**
     * Argument to represent the duration in milliseconds to press and hold a node.
     * <p>
     * <strong>Type:</strong> int<br>
     * <strong>Actions:</strong>
     * <ul>
     *     <li>{@link AccessibilityActionCompat#ACTION_PRESS_AND_HOLD}</li>
     * </ul>
     *
     * @see AccessibilityActionCompat#ACTION_PRESS_AND_HOLD
     */
    @SuppressLint("ActionValue")
    public static final String ACTION_ARGUMENT_PRESS_AND_HOLD_DURATION_MILLIS_INT =
            "android.view.accessibility.action.ARGUMENT_PRESS_AND_HOLD_DURATION_MILLIS_INT";

    /**
     * <p>Argument to represent the direction when using
     * {@link AccessibilityActionCompat#ACTION_SCROLL_IN_DIRECTION}.</p>
     *
     * <p>
     *     The value of this argument can be one of:
     *     <ul>
     *         <li>{@link View#FOCUS_DOWN}</li>
     *         <li>{@link View#FOCUS_UP}</li>
     *         <li>{@link View#FOCUS_LEFT}</li>
     *         <li>{@link View#FOCUS_RIGHT}</li>
     *         <li>{@link View#FOCUS_FORWARD}</li>
     *         <li>{@link View#FOCUS_BACKWARD}</li>
     *     </ul>
     * </p>
     */
    public static final String ACTION_ARGUMENT_DIRECTION_INT =
            "androidx.core.view.accessibility.action.ARGUMENT_DIRECTION_INT";

    /**
     * Argument to represent the scroll amount as a percent of the visible area of a node, with 1.0F
     * as the default. Values smaller than 1.0F represent a partial scroll of the node, and values
     * larger than 1.0F represent a scroll that extends beyond the currently visible node Rect.
     * Setting this to {@link Float#POSITIVE_INFINITY} or to another "too large" value should scroll
     * to the end of the node. Negative values should not be used with this argument.
     *
     * <p>This argument should be used with the following scroll actions:
     *
     * <ul>
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_FORWARD}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_BACKWARD}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_UP}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_DOWN}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_LEFT}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_RIGHT}
     * </ul>
     *
     * <p>Example: if a view representing a list of items implements {@link
     * AccessibilityActionCompat#ACTION_SCROLL_FORWARD} to scroll forward by an entire screen (one
     * "page"), then passing a value of .25F via this argument should scroll that view only by 1/4th
     * of a screen. Passing a value of 1.50F via this argument should scroll the view by 1 1/2
     * screens or to end of the node if the node doesn't extend to 1 1/2 screens.
     *
     * <p>This argument should not be used with the following scroll actions, which don't cleanly
     * conform to granular scroll semantics:
     *
     * <ul>
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_IN_DIRECTION}
     *   <li>{@link AccessibilityActionCompat#ACTION_SCROLL_TO_POSITION}
     * </ul>
     *
     * <p>Views that support this argument should set {@link
     * #setGranularScrollingSupported(boolean)} to true. Clients should use {@link
     * #isGranularScrollingSupported()} to check if granular scrolling is supported.
     */
    public static final String ACTION_ARGUMENT_SCROLL_AMOUNT_FLOAT =
            "androidx.core.view.accessibility.action.ARGUMENT_SCROLL_AMOUNT_FLOAT";

    // Checked states.

    /**
     * Checked state for a node that is not checked.
     *
     * @see #getChecked()
     * @see #setChecked(int)
     */
    public static final int CHECKED_STATE_FALSE = 0;

    /**
     * Checked state for a node that is checked.
     *
     * @see #getChecked()
     * @see #setChecked(int)
     */
    public static final int CHECKED_STATE_TRUE = 1;

    /**
     * Checked state for a node that is partially checked. For example, when a checkbox owns a
     * number of sub-options and they have different states, then the main checkbox is in a
     * partially-checked state.
     *
     * @see #getChecked()
     * @see #setChecked(int)
     */
    public static final int CHECKED_STATE_PARTIAL = 2;

    // Expanded states.

    /**
     * Expanded state for a non-expandable element
     *
     * @see #getExpandedState()
     * @see #setExpandedState(int)
     */
    public static final int EXPANDED_STATE_UNDEFINED = 0;

    /**
     * Expanded state for a collapsed expandable element.
     *
     * @see #getExpandedState()
     * @see #setExpandedState(int)
     */
    public static final int EXPANDED_STATE_COLLAPSED = 1;

    /**
     * Expanded state for an expanded expandable element that can still be expanded further.
     *
     * @see #getExpandedState()
     * @see #setExpandedState(int)
     */
    public static final int EXPANDED_STATE_PARTIAL = 2;

    /**
     * Expanded state for an expanded expandable element that cannot be expanded further.
     *
     * @see #getExpandedState()
     * @see #setExpandedState(int)
     */
    public static final int EXPANDED_STATE_FULL = 3;

    // Focus types

    /** The input focus. */
    public static final int FOCUS_INPUT = 1;

    /**
     * The accessibility focus.
     */
    public static final int FOCUS_ACCESSIBILITY = 2;

    // Movement granularities

    /**
     * Movement granularity bit for traversing the text of a node by character.
     */
    public static final int MOVEMENT_GRANULARITY_CHARACTER = 0x00000001;

    /**
     * Movement granularity bit for traversing the text of a node by word.
     */
    public static final int MOVEMENT_GRANULARITY_WORD = 0x00000002;

    /**
     * Movement granularity bit for traversing the text of a node by line.
     */
    public static final int MOVEMENT_GRANULARITY_LINE = 0x00000004;

    /**
     * Movement granularity bit for traversing the text of a node by paragraph.
     */
    public static final int MOVEMENT_GRANULARITY_PARAGRAPH = 0x00000008;

    /**
     * Movement granularity bit for traversing the text of a node by page.
     */
    public static final int MOVEMENT_GRANULARITY_PAGE = 0x00000010;

    /**
     * Key used to request and locate extra data for text character location. This key requests that
     * an array of {@link android.graphics.RectF}s be added to the extras. This request is made with
     * {@link android.view.accessibility.AccessibilityNodeInfo#refreshWithExtraData(String, Bundle)}.
     * The arguments taken by this request are two integers:
     * {@link #EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX} and
     * {@link #EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH}. The starting index must be valid
     * inside the CharSequence returned by {@link #getText()}, and the length must be positive.
     * <p>
     * The data can be retrieved from the {@code Bundle} returned by {@link #getExtras()} using this
     * string as a key for {@link Bundle#getParcelableArray(String)}. The
     * {@link android.graphics.RectF} will be {@code null} for characters that either do not exist
     * or are off the screen.
     * <p>
     * Note that character locations returned are modified by changes in display magnification.
     *
     * {@see android.view.accessibility.AccessibilityNodeInfo#refreshWithExtraData(String, Bundle)}
     */
    @SuppressWarnings("ActionValue")
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_KEY";

    /**
     * Key used to request and locate extra data for text character location in
     * window coordinates. This key requests that an array of
     * {@link android.graphics.RectF}s be added to the extras. This request is made with
     * {@link android.view.accessibility.AccessibilityNodeInfo#refreshWithExtraData(String, Bundle)}.
     * The arguments taken by this request are two integers:
     * {@link #EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX} and
     * {@link #EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH}. The starting index
     * must be valid inside the CharSequence returned by {@link #getText()}, and
     * the length must be positive.
     * <p>
     * Providers may advertise that they support text characters in window coordinates using
     * {@link #setAvailableExtraData(List)}. Services may check if an implementation supports text
     * characters in window coordinates with {@link #getAvailableExtraData()}.
     * <p>
     * The data can be retrieved from the {@code Bundle} returned by
     * {@link #getExtras()} using this string as a key for
     * {@link Bundle#getParcelableArray(String, Class)}. The
     * {@link android.graphics.RectF} will be {@code null} for characters that either do
     * not exist or are outside of the window bounds.
     * <p>
     * Note that character locations in window bounds are not modified by
     * changes in display magnification.
     *
     * {@see android.view.accessibility.AccessibilityNodeInfo#refreshWithExtraData(String, Bundle)}
     */
    @SuppressWarnings("ActionValue")
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_IN_WINDOW_KEY =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_IN_WINDOW_KEY";

    /**
     * Integer argument specifying the start index of the requested text location data. Must be
     * valid inside the CharSequence returned by {@link #getText()}.
     *
     * @see #EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY
     */
    @SuppressWarnings("ActionValue")
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX";

    /**
     * Integer argument specifying the end index of the requested text location data. Must be
     * positive and no larger than {@link #EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH}.
     *
     * @see #EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY
     */
    @SuppressWarnings("ActionValue")
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH";

    /**
     * The maximum allowed length of the requested text location data.
     */
    public static final int EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_MAX_LENGTH = 20000;

    /**
     * Prefetching strategy that prefetches the ancestors of the requested node.
     * <p> Ancestors will be prefetched before siblings and descendants.
     *
     * @see #getChild(int, int)
     * @see #getParent(int)
     * @see AccessibilityWindowInfoCompat#getRoot(int)
     * @see AccessibilityService#getRootInActiveWindow(int)
     * @see AccessibilityEvent#getSource(int)
     */
    public static final int FLAG_PREFETCH_ANCESTORS = 0x00000001;

    /**
     * Prefetching strategy that prefetches the siblings of the requested node.
     * <p> To avoid disconnected trees, this flag will also prefetch the parent. Siblings will be
     * prefetched before descendants.
     *
     * @see #FLAG_PREFETCH_ANCESTORS for where to use these flags.
     */
    public static final int FLAG_PREFETCH_SIBLINGS = 0x00000002;

    /**
     * Prefetching strategy that prefetches the descendants in a hybrid depth first and breadth
     * first approach.
     * <p> The children of the root node is prefetched before recursing on the children. This
     * must not be combined with {@link #FLAG_PREFETCH_DESCENDANTS_DEPTH_FIRST} or
     * {@link #FLAG_PREFETCH_DESCENDANTS_BREADTH_FIRST} or this will trigger an
     * IllegalArgumentException.
     *
     * @see #FLAG_PREFETCH_ANCESTORS for where to use these flags.
     */
    public static final int FLAG_PREFETCH_DESCENDANTS_HYBRID = 0x00000004;

    /**
     * Prefetching strategy that prefetches the descendants of the requested node depth-first.
     * <p> This must not be combined with {@link #FLAG_PREFETCH_DESCENDANTS_HYBRID} or
     * {@link #FLAG_PREFETCH_DESCENDANTS_BREADTH_FIRST} or this will trigger an
     * IllegalArgumentException.
     *
     * @see #FLAG_PREFETCH_ANCESTORS for where to use these flags.
     */
    public static final int FLAG_PREFETCH_DESCENDANTS_DEPTH_FIRST = 0x00000008;

    /**
     * Prefetching strategy that prefetches the descendants of the requested node breadth-first.
     * <p> This must not be combined with {@link #FLAG_PREFETCH_DESCENDANTS_HYBRID} or
     * {@link #FLAG_PREFETCH_DESCENDANTS_DEPTH_FIRST} or this will trigger an
     * IllegalArgumentException.
     *
     * @see #FLAG_PREFETCH_ANCESTORS for where to use these flags.
     */
    public static final int FLAG_PREFETCH_DESCENDANTS_BREADTH_FIRST = 0x00000010;

    /**
     * Prefetching flag that specifies prefetching should not be interrupted by a request to
     * retrieve a node or perform an action on a node.
     *
     * @see #FLAG_PREFETCH_ANCESTORS for where to use these flags.
     */
    public static final int FLAG_PREFETCH_UNINTERRUPTIBLE = 0x00000020;

    /**
     * Maximum batch size of prefetched nodes for a request.
     */
    @SuppressLint("MinMaxConstant")
    public static final int MAX_NUMBER_OF_PREFETCHED_NODES = 50;

    @IntDef(
            flag = false,
            value = {
                EXPANDED_STATE_UNDEFINED,
                EXPANDED_STATE_COLLAPSED,
                EXPANDED_STATE_PARTIAL,
                EXPANDED_STATE_FULL,
            })
    @RestrictTo(LIBRARY_GROUP_PREFIX)
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExpandedState {}

    @IntDef(
            flag = false,
            value = {
                CHECKED_STATE_FALSE,
                CHECKED_STATE_TRUE,
                CHECKED_STATE_PARTIAL,
            })
    @RestrictTo(LIBRARY_GROUP_PREFIX)
    @Retention(RetentionPolicy.SOURCE)
    public @interface CheckedState {}

    private static int sClickableSpanId = 0;

    /**
     * Creates a wrapper for info implementation.
     *
     * @param object The info to wrap.
     * @return A wrapper for if the object is not null, null otherwise.
     */
    @SuppressWarnings("deprecation")
    static AccessibilityNodeInfoCompat wrapNonNullInstance(Object object) {
        if (object != null) {
            return new AccessibilityNodeInfoCompat(object);
        }
        return null;
    }

    /**
     * Creates a new instance wrapping an
     * {@link android.view.accessibility.AccessibilityNodeInfo}.
     *
     * @param info The info.
     *
     * @deprecated Use {@link #wrap(AccessibilityNodeInfo)} instead.
     */
    @Deprecated
    public AccessibilityNodeInfoCompat(Object info) {
        mInfo = (AccessibilityNodeInfo) info;
    }

    private AccessibilityNodeInfoCompat(AccessibilityNodeInfo info) {
        mInfo = info;
    }

    /**
     * Creates a new instance wrapping an
     * {@link android.view.accessibility.AccessibilityNodeInfo}.
     *
     * @param info The info.
     */
    public static AccessibilityNodeInfoCompat wrap(@NonNull AccessibilityNodeInfo info) {
        return new AccessibilityNodeInfoCompat(info);
    }

    /**
     * @return The unwrapped {@link android.view.accessibility.AccessibilityNodeInfo}.
     */
    public AccessibilityNodeInfo unwrap() {
        return mInfo;
    }

    /**
     * @return The wrapped {@link android.view.accessibility.AccessibilityNodeInfo}.
     *
     * @deprecated Use {@link #unwrap()} instead.
     */
    @Deprecated
    public Object getInfo() {
        return mInfo;
    }

    /**
     * Returns a cached instance if such is available otherwise a new one and
     * sets the source.
     *
     * @return An instance.
     * @see #setSource(View)
     */
    public static AccessibilityNodeInfoCompat obtain(View source) {
        return AccessibilityNodeInfoCompat.wrap(AccessibilityNodeInfo.obtain(source));
    }

    /**
     * Returns a cached instance if such is available otherwise a new one
     * and sets the source.
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual descendant.
     * @return An instance.
     *
     * @see #setSource(View, int)
     */
    public static AccessibilityNodeInfoCompat obtain(View root, int virtualDescendantId) {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(
                AccessibilityNodeInfo.obtain(root, virtualDescendantId));
    }

    /**
     * Returns a cached instance if such is available otherwise a new one.
     *
     * @return An instance.
     */
    public static AccessibilityNodeInfoCompat obtain() {
        return AccessibilityNodeInfoCompat.wrap(AccessibilityNodeInfo.obtain());
    }

    /**
     * Returns a cached instance if such is available or a new one is create.
     * The returned instance is initialized from the given <code>info</code>.
     *
     * @param info The other info.
     * @return An instance.
     */
    public static AccessibilityNodeInfoCompat obtain(AccessibilityNodeInfoCompat info) {
        return AccessibilityNodeInfoCompat.wrap(AccessibilityNodeInfo.obtain(info.mInfo));
    }

    /**
     * Sets the source.
     *
     * @param source The info source.
     */
    public void setSource(View source) {
        mVirtualDescendantId = NO_ID;

        mInfo.setSource(source);
    }

    /**
     * Sets the source to be a virtual descendant of the given <code>root</code>.
     * If <code>virtualDescendantId</code> is {@link View#NO_ID} the root
     * is set as the source.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report themselves as a tree of virtual views, thus conveying their
     * logical structure.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}.
     * This class is made immutable before being delivered to an AccessibilityService.
     * <p>
     * This method is not supported on devices running API level < 16 since the platform did
     * not support virtual descendants of real views.
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void setSource(View root, int virtualDescendantId) {
        // Store the ID anyway, since we may need it for equality checks.
        mVirtualDescendantId = virtualDescendantId;

        mInfo.setSource(root, virtualDescendantId);
    }

    /**
     * Find the view that has the specified focus type. The search starts from
     * the view represented by this node info.
     *
     * @param focus The focus to find. One of {@link #FOCUS_INPUT} or
     *         {@link #FOCUS_ACCESSIBILITY}.
     * @return The node info of the focused view or null.
     *
     * @see #FOCUS_INPUT
     * @see #FOCUS_ACCESSIBILITY
     */
    public AccessibilityNodeInfoCompat findFocus(int focus) {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.findFocus(focus));
    }

    /**
     * Searches for the nearest view in the specified direction that can take
     * input focus.
     *
     * @param direction The direction. Can be one of:
     *     {@link View#FOCUS_DOWN},
     *     {@link View#FOCUS_UP},
     *     {@link View#FOCUS_LEFT},
     *     {@link View#FOCUS_RIGHT},
     *     {@link View#FOCUS_FORWARD},
     *     {@link View#FOCUS_BACKWARD}.
     *
     * @return The node info for the view that can take accessibility focus.
     */
    public AccessibilityNodeInfoCompat focusSearch(int direction) {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.focusSearch(direction));
    }

    /**
     * Gets the id of the window from which the info comes from.
     *
     * @return The window id.
     */
    public int getWindowId() {
        return mInfo.getWindowId();
    }

    /**
     * Gets the number of children.
     *
     * @return The child count.
     */
    public int getChildCount() {
        return mInfo.getChildCount();
    }

    /**
     * Get the child at given index.
     *
     * @param index The child index.
     * @return The child node.
     * @throws IllegalStateException If called outside of an
     *             AccessibilityService.
     */
    public AccessibilityNodeInfoCompat getChild(int index) {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getChild(index));
    }

    /**
     * Get the child at given index.
     *
     * @param index The child index.
     * @param prefetchingStrategy the prefetching strategy.
     * @return The child node.
     *
     * @throws IllegalStateException If called outside of an {@link AccessibilityService} and before
     *                               calling {@link #setQueryFromAppProcessEnabled}.
     *
     * @see AccessibilityNodeInfoCompat#getParent(int) for a description of prefetching.
     */
    public @Nullable AccessibilityNodeInfoCompat getChild(int index, int prefetchingStrategy) {
        if (Build.VERSION.SDK_INT >= 33) {
            return Api33Impl.getChild(mInfo, index, prefetchingStrategy);
        }
        return getChild(index);
    }

    /**
     * Adds a child.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param child The child.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void addChild(View child) {
        mInfo.addChild(child);
    }

    /**
     * Adds a virtual child which is a descendant of the given <code>root</code>.
     * If <code>virtualDescendantId</code> is {@link View#NO_ID} the root
     * is added as a child.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report them selves as a tree of virtual views, thus conveying their
     * logical structure.
     * </p>
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual child.
     */
    public void addChild(View root, int virtualDescendantId) {
        mInfo.addChild(root, virtualDescendantId);
    }

    /**
     * Removes a child. If the child was not previously added to the node,
     * calling this method has no effect.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}.
     * This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param child The child.
     * @return true if the child was present
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public boolean removeChild(View child) {
        return mInfo.removeChild(child);
    }

    /**
     * Removes a virtual child which is a descendant of the given
     * <code>root</code>. If the child was not previously added to the node,
     * calling this method has no effect.
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual child.
     * @return true if the child was present
     * @see #addChild(View, int)
     */
    public boolean removeChild(View root, int virtualDescendantId) {
        return mInfo.removeChild(root, virtualDescendantId);
    }

    /**
     * Gets the actions that can be performed on the node.
     *
     * @return The bit mask of with actions.
     * @see android.view.accessibility.AccessibilityNodeInfo#ACTION_FOCUS
     * @see android.view.accessibility.AccessibilityNodeInfo#ACTION_CLEAR_FOCUS
     * @see android.view.accessibility.AccessibilityNodeInfo#ACTION_SELECT
     * @see android.view.accessibility.AccessibilityNodeInfo#ACTION_CLEAR_SELECTION
     *
     * @deprecated Use {@link #getActionList()} instead.
     */
    @Deprecated
    public int getActions() {
        return mInfo.getActions();
    }

    /**
     * Adds an action that can be performed on the node.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param action The action.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void addAction(int action) {
        mInfo.addAction(action);
    }

    private List<Integer> extrasIntList(String key) {
        ArrayList<Integer> list = mInfo.getExtras()
                .getIntegerArrayList(key);
        if (list == null) {
            list = new ArrayList<Integer>();
            mInfo.getExtras().putIntegerArrayList(key, list);
        }
        return list;
    }

    /**
     * Adds an action that can be performed on the node.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param action The action.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void addAction(AccessibilityActionCompat action) {
        mInfo.addAction((AccessibilityNodeInfo.AccessibilityAction) action.mAction);
    }

    /**
     * Removes an action that can be performed on the node. If the action was
     * not already added to the node, calling this method has no effect.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param action The action to be removed.
     * @return The action removed from the list of actions.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public boolean removeAction(AccessibilityActionCompat action) {
        return mInfo.removeAction((AccessibilityNodeInfo.AccessibilityAction) action.mAction);
    }

    /**
     * Performs an action on the node.
     * <p>
     * <strong>Note:</strong> An action can be performed only if the request is
     * made from an {@link android.accessibilityservice.AccessibilityService}.
     * </p>
     *
     * @param action The action to perform.
     * @return True if the action was performed.
     * @throws IllegalStateException If called outside of an
     *             AccessibilityService.
     */
    public boolean performAction(int action) {
        return mInfo.performAction(action);
    }

    /**
     * Performs an action on the node.
     * <p>
     *   <strong>Note:</strong> An action can be performed only if the request is made
     *   from an {@link android.accessibilityservice.AccessibilityService}.
     * </p>
     *
     * @param action The action to perform.
     * @param arguments A bundle with additional arguments.
     * @return True if the action was performed.
     *
     * @throws IllegalStateException If called outside of an AccessibilityService.
     */
    public boolean performAction(int action, Bundle arguments) {
        return mInfo.performAction(action, arguments);
    }

    /**
     * Sets the movement granularities for traversing the text of this node.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param granularities The bit mask with granularities.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setMovementGranularities(int granularities) {
        mInfo.setMovementGranularities(granularities);
    }

    /**
     * Gets the movement granularities for traversing the text of this node.
     *
     * @return The bit mask with granularities.
     */
    public int getMovementGranularities() {
        return mInfo.getMovementGranularities();
    }

    /**
     * Gets the expanded state for this node.
     *
     * @return The expanded state, one of:
     *     <ul>
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_UNDEFINED}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_COLLAPSED}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_FULL}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_PARTIAL}
     *     </ul>
     */
    @ExpandedState
    public int getExpandedState() {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.getExpandedState(mInfo);
        } else {
            return mInfo.getExtras().getInt(EXPANDED_STATE_KEY, EXPANDED_STATE_UNDEFINED);
        }
    }

    /**
     * Sets the expanded state of the node.
     *
     * <p><strong>Note:</strong> Cannot be called from an {@link
     * android.accessibilityservice.AccessibilityService}. This class is made immutable before being
     * delivered to an {@link android.accessibilityservice.AccessibilityService}.
     *
     * @param state new expanded state of this node.
     * @throws IllegalArgumentException If state is not one of:
     *     <ul>
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_UNDEFINED}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_COLLAPSED}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_PARTIAL}
     *       <li>{@link AccessibilityNodeInfoCompat#EXPANDED_STATE_FULL}
     *     </ul>
     *
     * @throws IllegalStateException If called from an AccessibilityService
     */
    public void setExpandedState(@ExpandedState int state) {
        if (Build.VERSION.SDK_INT >= 36) {
            Api36Impl.setExpandedState(mInfo, state);
        } else {
            mInfo.getExtras().putInt(EXPANDED_STATE_KEY, state);
        }
    }

    /**
     * Finds {@link android.view.accessibility.AccessibilityNodeInfo}s by text. The match
     * is case insensitive containment. The search is relative to this info i.e. this
     * info is the root of the traversed tree.
     *
     * @param text The searched text.
     * @return A list of node info.
     */
    public List<AccessibilityNodeInfoCompat> findAccessibilityNodeInfosByText(String text) {
        List<AccessibilityNodeInfoCompat> result = new ArrayList<AccessibilityNodeInfoCompat>();
        List<AccessibilityNodeInfo> infos = mInfo.findAccessibilityNodeInfosByText(text);
        final int infoCount = infos.size();
        for (int i = 0; i < infoCount; i++) {
            AccessibilityNodeInfo info = infos.get(i);
            result.add(AccessibilityNodeInfoCompat.wrap(info));
        }
        return result;
    }

    /**
     * Gets the parent.
     *
     * @return The parent.
     */
    public AccessibilityNodeInfoCompat getParent() {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getParent());
    }

    /**
     * Gets the parent.
     *
     * <p>
     * Use {@code prefetchingStrategy} to determine the types of
     * nodes prefetched from the app if the requested node is not in the cache and must be retrieved
     * by the app. The default strategy for {@link #getParent()} is a combination of ancestor and
     * sibling strategies. The app will prefetch until all nodes fulfilling the strategies are
     * fetched, another node request is sent, or the maximum prefetch batch size of
     * {@link #MAX_NUMBER_OF_PREFETCHED_NODES} nodes is reached. To prevent interruption by another
     * request and to force prefetching of the max batch size, use
     * {@link AccessibilityNodeInfoCompat#FLAG_PREFETCH_UNINTERRUPTIBLE}.
     * </p>
     *
     * @param prefetchingStrategy the prefetching strategy.
     * @return The parent.
     *
     * @throws IllegalStateException If called outside of an {@link AccessibilityService} and before
     *                               calling {@link #setQueryFromAppProcessEnabled}.
     *
     * @see #FLAG_PREFETCH_ANCESTORS
     * @see #FLAG_PREFETCH_DESCENDANTS_BREADTH_FIRST
     * @see #FLAG_PREFETCH_DESCENDANTS_DEPTH_FIRST
     * @see #FLAG_PREFETCH_DESCENDANTS_HYBRID
     * @see #FLAG_PREFETCH_SIBLINGS
     * @see #FLAG_PREFETCH_UNINTERRUPTIBLE
     */
    public @Nullable AccessibilityNodeInfoCompat getParent(int prefetchingStrategy) {
        if (Build.VERSION.SDK_INT >= 33) {
            return Api33Impl.getParent(mInfo, prefetchingStrategy);
        }
        return getParent();
    }

    /**
     * Sets the parent.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param parent The parent.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setParent(View parent) {
        mParentVirtualDescendantId = NO_ID;

        mInfo.setParent(parent);
    }

    /**
     * Sets the parent to be a virtual descendant of the given <code>root</code>.
     * If <code>virtualDescendantId</code> equals to {@link View#NO_ID} the root
     * is set as the parent.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report them selves as a tree of virtual views, thus conveying their
     * logical structure.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}.
     * This class is made immutable before being delivered to an AccessibilityService.
     * <p>
     * This method is not supported on devices running API level < 16 since the platform did
     * not support virtual descendants of real views.
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void setParent(View root, int virtualDescendantId) {
        // Store the ID anyway, since we may need it for equality checks.
        mParentVirtualDescendantId = virtualDescendantId;

        mInfo.setParent(root, virtualDescendantId);
    }

    /**
     * Gets the node bounds in the viewParent's coordinates.
     * {@link #getParent()} does not represent the source's viewParent.
     * Instead it represents the result of {@link View#getParentForAccessibility()},
     * which returns the closest ancestor where {@link View#isImportantForAccessibility()} is true.
     * So this method is not reliable.
     *
     * @param outBounds The output node bounds.
     *
     * @deprecated Use {@link #getBoundsInScreen(Rect)} instead.
     */
    @Deprecated
    public void getBoundsInParent(Rect outBounds) {
        mInfo.getBoundsInParent(outBounds);
    }

    /**
     * Sets the node bounds in the viewParent's coordinates.
     * {@link #getParent()} does not represent the source's viewParent.
     * Instead it represents the result of {@link View#getParentForAccessibility()},
     * which returns the closest ancestor where {@link View#isImportantForAccessibility()} is true.
     * So this method is not reliable.
     *
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param bounds The node bounds.
     * @throws IllegalStateException If called from an AccessibilityService.
     *
     * @deprecated Accessibility services should not care about these bounds.
     */
    @Deprecated
    public void setBoundsInParent(Rect bounds) {
        mInfo.setBoundsInParent(bounds);
    }

    /**
     * Gets the node bounds in screen coordinates.
     *
     * @param outBounds The output node bounds.
     */
    public void getBoundsInScreen(Rect outBounds) {
        mInfo.getBoundsInScreen(outBounds);
    }

    /**
     * Sets the node bounds in screen coordinates.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param bounds The node bounds.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setBoundsInScreen(Rect bounds) {
        mInfo.setBoundsInScreen(bounds);
    }

    /**
     * Gets the node bounds in window coordinates.
     * <p>
     * When magnification is enabled, the bounds in window are scaled up by magnification scale
     * and the positions are also adjusted according to the offset of magnification viewport.
     * For example, it returns Rect(-180, -180, 0, 0) for original bounds Rect(10, 10, 100, 100),
     * when the magnification scale is 2 and offsets for X and Y are both 200.
     * <p/>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 19: No-op</li>
     * </ul>
     * @param outBounds The output node bounds.
     */
    public void getBoundsInWindow(@NonNull Rect outBounds) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.getBoundsInWindow(mInfo, outBounds);
        } else {
            Rect extraBounds = mInfo.getExtras().getParcelable(BOUNDS_IN_WINDOW_KEY);
            if (extraBounds != null) {
                outBounds.set(extraBounds.left, extraBounds.top, extraBounds.right,
                        extraBounds.bottom);
            }
        }
    }

    /**
     * Sets the node bounds in window coordinates.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 19: No-op</li>
     * </ul>
     * @param bounds The node bounds.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setBoundsInWindow(@NonNull Rect bounds) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setBoundsInWindow(mInfo, bounds);
        } else {
            mInfo.getExtras().putParcelable(BOUNDS_IN_WINDOW_KEY, bounds);
        }
    }

    /**
     * Gets whether this node is checkable.
     *
     * @return True if the node is checkable.
     */
    public boolean isCheckable() {
        return mInfo.isCheckable();
    }

    /**
     * Sets whether this node is checkable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param checkable True if the node is checkable.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setCheckable(boolean checkable) {
        mInfo.setCheckable(checkable);
    }

    /**
     * Gets whether this node is checked.
     *
     * @return True if the node is checked.
     *
     * @deprecated Use {@link #getChecked()} instead.
     */
    @Deprecated
    public boolean isChecked() {
        return mInfo.isChecked();
    }

    /**
     * Sets whether this node is checked.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param checked True if the node is checked.
     * @throws IllegalStateException If called from an AccessibilityService.
     *
     * @deprecated Use {@link #setChecked(int)} instead.
     */
    @Deprecated
    public void setChecked(boolean checked) {
        mInfo.setChecked(checked);
    }

    /**
     * Gets the checked state of this node.
     *
     * <p>Note that this is only meaningful when {@link #isCheckable()} returns {@code true}.
     *
     * @see #setCheckable(boolean)
     * @see #isCheckable()
     * @see #setChecked(int)
     * @return The checked state, one of:
     *     <ul>
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_FALSE}
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_TRUE}
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_PARTIAL}
     *     </ul>
     */
    @CheckedState
    public int getChecked() {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.getChecked(mInfo);
        } else {
            return mInfo.getExtras()
                    .getInt(
                            CHECKED_KEY,
                            mInfo.isChecked() ? CHECKED_STATE_TRUE : CHECKED_STATE_FALSE);
        }
    }

    /**
     * Sets the checked state of this node. This is only meaningful when {@link #isCheckable()}
     * returns {@code true}.
     *
     * <p><strong>Note:</strong> Cannot be called from an {@link
     * android.accessibilityservice.AccessibilityService}. This class is made immutable before being
     * delivered to an AccessibilityService.
     *
     * @see #setCheckable(boolean)
     * @see #isCheckable()
     * @see #getChecked()
     * @param checked The checked state. One of
     *     <ul>
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_FALSE}
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_TRUE}
     *       <li>{@link AccessibilityNodeInfoCompat#CHECKED_STATE_PARTIAL}
     *     </ul>
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     * @throws IllegalArgumentException if {@code checked} is not one of {@link
     *     AccessibilityNodeInfoCompat#CHECKED_STATE_FALSE}, {@link
     *     AccessibilityNodeInfoCompat#CHECKED_STATE_TRUE}, or {@link
     *     AccessibilityNodeInfoCompat#CHECKED_STATE_PARTIAL}.
     */
    public void setChecked(@CheckedState int checked) {
        if (Build.VERSION.SDK_INT >= 36) {
            Api36Impl.setChecked(mInfo, checked);
            return;
        }

        if (checked == CHECKED_STATE_TRUE
                || checked == CHECKED_STATE_PARTIAL
                || checked == CHECKED_STATE_FALSE) {
            mInfo.setChecked(checked == CHECKED_STATE_TRUE);
            mInfo.getExtras().putInt(CHECKED_KEY, checked);
        } else {
            throw new IllegalArgumentException("Unknown checked argument: " + checked);
        }
    }

    /**
     * Gets whether a node representing a form field requires input or selection.
     *
     * @return {@code true} if {@code this} node represents a form field that requires input or
     *     selection, {@code false} otherwise.
     */
    public boolean isFieldRequired() {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.isFieldRequired(mInfo);
        } else {
            return mInfo.getExtras().getBoolean(IS_REQUIRED_KEY);
        }
    }

    /**
     * Sets whether {@code this} node represents a form field that requires input or selection.
     *
     * <p><strong>Note:</strong> Cannot be called from an AccessibilityService. This class is made
     * immutable before being delivered to an AccessibilityService.
     *
     * @param required {@code true} if input or selection of this node should be required, {@code
     *     false} otherwise.
     * @throws IllegalStateException If called from an AccessibilityService
     */
    public void setFieldRequired(boolean required) {
        if (Build.VERSION.SDK_INT >= 36) {
            Api36Impl.setFieldRequired(mInfo, required);
        } else {
            mInfo.getExtras().putBoolean(IS_REQUIRED_KEY, required);
        }
    }

    /**
     * Gets whether this node is focusable.
     *
     * @return True if the node is focusable.
     */
    public boolean isFocusable() {
        return mInfo.isFocusable();
    }

    /**
     * Sets whether this node is focusable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param focusable True if the node is focusable.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setFocusable(boolean focusable) {
        mInfo.setFocusable(focusable);
    }

    /**
     * Gets whether this node is focused.
     *
     * @return True if the node is focused.
     */
    public boolean isFocused() {
        return mInfo.isFocused();
    }

    /**
     * Sets whether this node is focused.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param focused True if the node is focused.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setFocused(boolean focused) {
        mInfo.setFocused(focused);
    }

    /**
     * Gets whether this node is visible to the user.
     *
     * @return Whether the node is visible to the user.
     */
    public boolean isVisibleToUser() {
        return mInfo.isVisibleToUser();
    }

    /**
     * Sets whether this node is visible to the user.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param visibleToUser Whether the node is visible to the user.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setVisibleToUser(boolean visibleToUser) {
        mInfo.setVisibleToUser(visibleToUser);
    }

    /**
     * Gets whether this node is accessibility focused.
     *
     * @return True if the node is accessibility focused.
     */
    public boolean isAccessibilityFocused() {
        return mInfo.isAccessibilityFocused();
    }

    /**
     * Sets whether this node is accessibility focused.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param focused True if the node is accessibility focused.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setAccessibilityFocused(boolean focused) {
        mInfo.setAccessibilityFocused(focused);
    }

    /**
     * Gets whether this node is selected.
     *
     * @return True if the node is selected.
     */
    public boolean isSelected() {
        return mInfo.isSelected();
    }

    /**
     * Sets whether this node is selected.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param selected True if the node is selected.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setSelected(boolean selected) {
        mInfo.setSelected(selected);
    }

    /**
     * Gets whether this node is clickable.
     *
     * @return True if the node is clickable.
     */
    public boolean isClickable() {
        return mInfo.isClickable();
    }

    /**
     * Sets whether this node is clickable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param clickable True if the node is clickable.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setClickable(boolean clickable) {
        mInfo.setClickable(clickable);
    }

    /**
     * Gets whether this node is long clickable.
     *
     * @return True if the node is long clickable.
     */
    public boolean isLongClickable() {
        return mInfo.isLongClickable();
    }

    /**
     * Sets whether this node is long clickable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param longClickable True if the node is long clickable.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setLongClickable(boolean longClickable) {
        mInfo.setLongClickable(longClickable);
    }

    /**
     * Gets whether this node is enabled.
     *
     * @return True if the node is enabled.
     */
    public boolean isEnabled() {
        return mInfo.isEnabled();
    }

    /**
     * Sets whether this node is enabled.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param enabled True if the node is enabled.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setEnabled(boolean enabled) {
        mInfo.setEnabled(enabled);
    }

    /**
     * Gets whether this node is a password.
     *
     * @return True if the node is a password.
     */
    public boolean isPassword() {
        return mInfo.isPassword();
    }

    /**
     * Sets whether this node is a password.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param password True if the node is a password.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setPassword(boolean password) {
        mInfo.setPassword(password);
    }

    /**
     * Gets if the node is scrollable.
     *
     * @return True if the node is scrollable, false otherwise.
     */
    public boolean isScrollable() {
        return mInfo.isScrollable();
    }

    /**
     * Sets if the node is scrollable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param scrollable True if the node is scrollable, false otherwise.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setScrollable(boolean scrollable) {
        mInfo.setScrollable(scrollable);
    }


    /**
     * Gets if the node supports granular scrolling.
     * <p>
     * Compatibility:
     * <ul>
     *     <li>Api &lt; 19: Returns false.</li>
     * </ul>
     * @return True if all scroll actions that could support
     * {@link #ACTION_ARGUMENT_SCROLL_AMOUNT_FLOAT} have done so, false otherwise.
     */
    public boolean isGranularScrollingSupported() {
        return getBooleanProperty(BOOLEAN_PROPERTY_SUPPORTS_GRANULAR_SCROLLING);
    }

    /**
     * Sets if the node supports granular scrolling. This should be set to true if all scroll
     * actions which could support {@link #ACTION_ARGUMENT_SCROLL_AMOUNT_FLOAT} have done so.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>Api &lt; 19: No-op.</li>
     * </ul>
     * @param granularScrollingSupported True if the node supports granular scrolling, false
     *                                  otherwise.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setGranularScrollingSupported(boolean granularScrollingSupported) {
        setBooleanProperty(BOOLEAN_PROPERTY_SUPPORTS_GRANULAR_SCROLLING,
                granularScrollingSupported);
    }

    /**
     * Gets if the node has selectable text.
     *
     * <p>
     *     Services should use {@link #ACTION_SET_SELECTION} for selection. Editable text nodes must
     *     also be selectable. But not all UIs will populate this field, so services should consider
     *     'isTextSelectable | isEditable' to ensure they don't miss nodes with selectable text.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>Api &lt; 19: Returns false.</li>
     * </ul>
     *
     * @see #isEditable
     * @return True if the node has selectable text.
     */
    public boolean isTextSelectable() {
        if (Build.VERSION.SDK_INT >= 33) {
            return Api33Impl.isTextSelectable(mInfo);
        } else {
            return getBooleanProperty(BOOLEAN_PROPERTY_TEXT_SELECTABLE);
        }
    }

    /**
     * Sets if the node has selectable text.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>Api &lt; 19: Does not operate.</li>
     * </ul>
     * </p>
     *
     * @param selectableText True if the node has selectable text, false otherwise.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setTextSelectable(boolean selectableText) {
        if (Build.VERSION.SDK_INT >= 33) {
            Api33Impl.setTextSelectable(mInfo, selectableText);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_TEXT_SELECTABLE, selectableText);
        }
    }

    /**
     * Gets the minimum time duration between two content change events.
     */
    public long getMinDurationBetweenContentChangesMillis() {
        if (Build.VERSION.SDK_INT >= 34) {
            return Api34Impl.getMinDurationBetweenContentChangeMillis(mInfo);
        } else {
            return mInfo.getExtras().getLong(MIN_DURATION_BETWEEN_CONTENT_CHANGES_KEY);
        }
    }

    /**
     * Sets the minimum time duration between two content change events, which is used in throttling
     * content change events in accessibility services.
     *
     * <p>
     * Example: An app can set MinDurationBetweenContentChanges as 1 min for a view which sends
     * content change events to accessibility services one event per second.
     * Accessibility service will throttle those content change events and only handle one event
     * per minute for that view.
     * </p>
     *
     * @see AccessibilityEventCompat#getContentChangeTypes for all content change types.
     * @param duration the minimum duration between content change events.
     */
    public void setMinDurationBetweenContentChangesMillis(long duration) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setMinDurationBetweenContentChangeMillis(mInfo, duration);
        } else {
            mInfo.getExtras().putLong(MIN_DURATION_BETWEEN_CONTENT_CHANGES_KEY, duration);
        }
    }

    /**
     * Returns whether the node originates from a view considered important for accessibility.
     *
     * @return {@code true} if the node originates from a view considered important for
     *         accessibility, {@code false} otherwise
     *
     * @see View#isImportantForAccessibility()
     */
    public boolean isImportantForAccessibility() {
        if (Build.VERSION.SDK_INT >= 24) {
            return mInfo.isImportantForAccessibility();
        } else {
            return true;
        }
    }

    /**
     * Sets whether the node is considered important for accessibility.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param important {@code true} if the node is considered important for accessibility,
     *                  {@code false} otherwise
     */
    public void setImportantForAccessibility(boolean important) {
        if (Build.VERSION.SDK_INT >= 24) {
            mInfo.setImportantForAccessibility(important);
        }
    }

    /**
     * Gets if the node's accessibility data is considered sensitive.
     *
     * @return True if the node's data is considered sensitive, false otherwise.
     * @see View#isAccessibilityDataSensitive()
     */
    public boolean isAccessibilityDataSensitive() {
        if (Build.VERSION.SDK_INT >= 34) {
            return Api34Impl.isAccessibilityDataSensitive(mInfo);
        } else {
            return getBooleanProperty(BOOLEAN_PROPERTY_ACCESSIBILITY_DATA_SENSITIVE);
        }
    }

    /**
     * Sets whether this node's accessibility data is considered sensitive.
     *
     * <p>
     * For SDK 34 and higher: when set to true the framework will hide this node from
     * accessibility services with the
     * {@link android.accessibilityservice.AccessibilityServiceInfo#isAccessibilityTool}
     * property set to false.
     * </p>
     * <p>
     * Otherwise, for SDK 19 and higher: the framework cannot hide this node but this property may
     * be read by accessibility services to provide modified behavior for sensitive nodes.
     * </p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param accessibilityDataSensitive True if the node's accessibility data is considered
     *                                   sensitive.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setAccessibilityDataSensitive(boolean accessibilityDataSensitive) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setAccessibilityDataSensitive(mInfo, accessibilityDataSensitive);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_ACCESSIBILITY_DATA_SENSITIVE,
                    accessibilityDataSensitive);
        }
    }

    /**
     * Gets the package this node comes from.
     *
     * @return The package name.
     */
    public CharSequence getPackageName() {
        return mInfo.getPackageName();
    }

    /**
     * Sets the package this node comes from.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param packageName The package name.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setPackageName(CharSequence packageName) {
        mInfo.setPackageName(packageName);
    }

    /**
     * Gets the class this node comes from.
     *
     * @return The class name.
     */
    public CharSequence getClassName() {
        return mInfo.getClassName();
    }

    /**
     * Sets the class this node comes from.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param className The class name.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setClassName(CharSequence className) {
        mInfo.setClassName(className);
    }

    /**
     * Gets the text of this node.
     *
     * @return The text.
     */
    public CharSequence getText() {
        if (hasSpans()) {
            List<Integer> starts = extrasIntList(SPANS_START_KEY);
            List<Integer> ends = extrasIntList(SPANS_END_KEY);
            List<Integer> flags = extrasIntList(SPANS_FLAGS_KEY);
            List<Integer> ids = extrasIntList(SPANS_ID_KEY);
            Spannable spannable = new SpannableString(TextUtils.substring(mInfo.getText(),
                    0, mInfo.getText().length()));
            for (int i = 0; i < starts.size(); i++) {
                spannable.setSpan(new AccessibilityClickableSpanCompat(ids.get(i), this,
                                getExtras().getInt(SPANS_ACTION_ID_KEY)),
                        starts.get(i), ends.get(i), flags.get(i));
            }
            return spannable;
        } else {
            return mInfo.getText();
        }
    }

    /**
     * Sets the text of this node.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param text The text.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setText(CharSequence text) {
        mInfo.setText(text);
    }

    /**
     */
    @RestrictTo(LIBRARY_GROUP_PREFIX)
    public void addSpansToExtras(CharSequence text, View view) {
        if (Build.VERSION.SDK_INT < 26) {
            clearExtrasSpans();
            removeCollectedSpans(view);
            ClickableSpan[] spans = getClickableSpans(text);
            if (spans != null && spans.length > 0) {
                getExtras().putInt(SPANS_ACTION_ID_KEY, R.id.accessibility_action_clickable_span);
                SparseArray<WeakReference<ClickableSpan>> tagSpans =
                        getOrCreateSpansFromViewTags(view);
                for (int i = 0; spans != null && i < spans.length; i++) {
                    int id = idForClickableSpan(spans[i], tagSpans);
                    tagSpans.put(id, new WeakReference<>(spans[i]));
                    addSpanLocationToExtras(spans[i], (Spanned) text, id);
                }
            }
        }
    }

    private SparseArray<WeakReference<ClickableSpan>> getOrCreateSpansFromViewTags(View host) {
        SparseArray<WeakReference<ClickableSpan>> spans = getSpansFromViewTags(host);
        if (spans == null) {
            spans = new SparseArray<>();
            host.setTag(R.id.tag_accessibility_clickable_spans, spans);
        }
        return spans;
    }

    @SuppressWarnings("unchecked")
    private SparseArray<WeakReference<ClickableSpan>> getSpansFromViewTags(View host) {
        return (SparseArray<WeakReference<ClickableSpan>>) host.getTag(
                R.id.tag_accessibility_clickable_spans);
    }

    /**
     */
    @RestrictTo(LIBRARY_GROUP_PREFIX)
    public static ClickableSpan[] getClickableSpans(CharSequence text) {
        if (text instanceof Spanned) {
            Spanned spanned = (Spanned) text;
            return spanned.getSpans(0, text.length(), ClickableSpan.class);
        }
        return null;
    }

    private int idForClickableSpan(ClickableSpan span,
            SparseArray<WeakReference<ClickableSpan>> spans) {
        if (spans != null) {
            for (int i = 0; i < spans.size(); i++) {
                ClickableSpan aSpan = spans.valueAt(i).get();
                if (span.equals(aSpan)) {
                    return spans.keyAt(i);
                }
            }
        }
        return sClickableSpanId++;
    }

    private boolean hasSpans() {
        return !extrasIntList(SPANS_START_KEY).isEmpty();
    }

    private void clearExtrasSpans() {
        mInfo.getExtras().remove(SPANS_START_KEY);
        mInfo.getExtras().remove(SPANS_END_KEY);
        mInfo.getExtras().remove(SPANS_FLAGS_KEY);
        mInfo.getExtras().remove(SPANS_ID_KEY);
    }

    private void addSpanLocationToExtras(ClickableSpan span, Spanned spanned, int id) {
        extrasIntList(SPANS_START_KEY).add(spanned.getSpanStart(span));
        extrasIntList(SPANS_END_KEY).add(spanned.getSpanEnd(span));
        extrasIntList(SPANS_FLAGS_KEY).add(spanned.getSpanFlags(span));
        extrasIntList(SPANS_ID_KEY).add(id);
    }

    private void removeCollectedSpans(View view) {
        SparseArray<WeakReference<ClickableSpan>> spans = getSpansFromViewTags(view);
        if (spans != null) {
            List<Integer> toBeRemovedIndices = new ArrayList<>();
            for (int i = 0; i < spans.size(); i++) {
                if (spans.valueAt(i).get() == null) {
                    toBeRemovedIndices.add(i);
                }
            }
            for (int i = 0; i < toBeRemovedIndices.size(); i++) {
                spans.remove(toBeRemovedIndices.get(i));
            }
        }
    }

    /**
     * Gets the content description of this node.
     *
     * @return The content description.
     */
    public CharSequence getContentDescription() {
        return mInfo.getContentDescription();
    }

    /**
     * Gets the state description of this node.
     *
     * @return the state description or null if android version smaller
     * than 19.
     */
    public @Nullable CharSequence getStateDescription() {
        if (Build.VERSION.SDK_INT >= 30) {
            return Api30Impl.getStateDescription(mInfo);
        } else {
            return mInfo.getExtras().getCharSequence(STATE_DESCRIPTION_KEY);
        }
    }

    /**
     * Sets the content description of this node.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param contentDescription The content description.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setContentDescription(CharSequence contentDescription) {
        mInfo.setContentDescription(contentDescription);
    }

    /**
     * Sets the state description of this node.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param stateDescription the state description of this node.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setStateDescription(@Nullable CharSequence stateDescription) {
        if (Build.VERSION.SDK_INT >= 30) {
            Api30Impl.setStateDescription(mInfo, stateDescription);
        } else {
            mInfo.getExtras().putCharSequence(STATE_DESCRIPTION_KEY, stateDescription);
        }
    }

    /**
     * Gets the unique id of this node.
     *
     * @return the unique id or null if android version smaller
     * than 19.
     */
    public @Nullable String getUniqueId() {
        if (Build.VERSION.SDK_INT >= 33) {
            return Api33Impl.getUniqueId(mInfo);
        } else {
            return mInfo.getExtras().getString(UNIQUE_ID_KEY);
        }
    }

    /**
     * Sets the unique id of this node.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param uniqueId the unique id of this node.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setUniqueId(@Nullable String uniqueId) {
        if (Build.VERSION.SDK_INT >= 33) {
            Api33Impl.setUniqueId(mInfo, uniqueId);
        } else {
            mInfo.getExtras().putString(UNIQUE_ID_KEY, uniqueId);
        }
    }

    /**
     * Sets the container title for app-developer-defined container which can be any type of
     * ViewGroup or layout.
     * Container title will be used to group together related controls, similar to HTML fieldset.
     * Or container title may identify a large piece of the UI that is visibly grouped together,
     * such as a toolbar or a card, etc.
     * <p>
     * Container title helps to assist in navigation across containers and other groups.
     * For example, a screen reader may use this to determine where to put accessibility focus.
     * </p>
     * <p>
     * Container title is different from pane title{@link #setPaneTitle} which indicates that the
     * node represents a window or activity.
     * </p>
     *
     * <p>
     *  Example: An app can set container titles on several non-modal menus, containing TextViews
     *  or ImageButtons that have content descriptions, text, etc. Screen readers can quickly
     *  switch accessibility focus among menus instead of child views.  Other accessibility-services
     *  can easily find the menu.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 19: No-op</li>
     * </ul>
     * @param containerTitle The container title that is associated with a ViewGroup/Layout on the
     *                       screen.
     */
    public void setContainerTitle(@Nullable CharSequence containerTitle) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setContainerTitle(mInfo, containerTitle);
        } else {
            mInfo.getExtras().putCharSequence(CONTAINER_TITLE_KEY, containerTitle);
        }
    }

    /**
     * Returns the container title.
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 19: Returns null</li>
     * </ul>
     * @see #setContainerTitle for details.
     */
    public @Nullable CharSequence getContainerTitle() {
        if (Build.VERSION.SDK_INT >= 34) {
            return Api34Impl.getContainerTitle(mInfo);
        } else {
            return mInfo.getExtras().getCharSequence(CONTAINER_TITLE_KEY);
        }
    }

    /**
     * Return an instance back to be reused.
     * <p>
     * <strong>Note:</strong> You must not touch the object after calling this function.
     *
     * @throws IllegalStateException If the info is already recycled.
     * @deprecated Accessibility Object recycling is no longer necessary or functional.
     */
    @Deprecated
    public void recycle() { }

    /**
     * Sets the fully qualified resource name of the source view's id.
     *
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param viewId The id resource name.
     */
    public void setViewIdResourceName(String viewId) {
        mInfo.setViewIdResourceName(viewId);
    }

    /**
     * Gets the fully qualified resource name of the source view's id.
     *
     * <p>
     *   <strong>Note:</strong> The primary usage of this API is for UI test automation
     *   and in order to report the source view id of an {@link AccessibilityNodeInfoCompat}
     *   the client has to set the {@link AccessibilityServiceInfoCompat#FLAG_REPORT_VIEW_IDS}
     *   flag when configuring their {@link android.accessibilityservice.AccessibilityService}.
     * </p>
     *
     * @return The id resource name.
     */
    public String getViewIdResourceName() {
        return mInfo.getViewIdResourceName();
    }

    /**
     * Gets the node's live region mode.
     * <p>
     * A live region is a node that contains information that is important for
     * the user and when it changes the user should be notified. For example,
     * in a login screen with a TextView that displays an "incorrect password"
     * notification, that view should be marked as a live region with mode
     * {@link ViewCompat#ACCESSIBILITY_LIVE_REGION_POLITE}.
     * <p>
     * It is the responsibility of the accessibility service to monitor
     * {@link AccessibilityEventCompat#TYPE_WINDOW_CONTENT_CHANGED} events
     * indicating changes to live region nodes and their children.
     *
     * @return The live region mode, or
     *         {@link ViewCompat#ACCESSIBILITY_LIVE_REGION_NONE} if the view is
     *         not a live region.
     * @see ViewCompat#getAccessibilityLiveRegion(View)
     */
    public int getLiveRegion() {
        return mInfo.getLiveRegion();
    }

    /**
     * Sets the node's live region mode.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is
     * made immutable before being delivered to an AccessibilityService.
     *
     * @param mode The live region mode, or
     *        {@link ViewCompat#ACCESSIBILITY_LIVE_REGION_NONE} if the view is
     *        not a live region.
     * @see ViewCompat#setAccessibilityLiveRegion(View, int)
     */
    public void setLiveRegion(int mode) {
        mInfo.setLiveRegion(mode);
    }

    /**
     * Get the drawing order of the view corresponding it this node.
     * <p>
     * Drawing order is determined only within the node's parent, so this index is only relative
     * to its siblings.
     * <p>
     * In some cases, the drawing order is essentially simultaneous, so it is possible for two
     * siblings to return the same value. It is also possible that values will be skipped.
     *
     * @return The drawing position of the view corresponding to this node relative to its siblings.
     */
    public int getDrawingOrder() {
        if (Build.VERSION.SDK_INT >= 24) {
            return mInfo.getDrawingOrder();
        } else {
            return 0;
        }
    }

    /**
     * Set the drawing order of the view corresponding it this node.
     *
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * @param drawingOrderInParent
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setDrawingOrder(int drawingOrderInParent) {
        if (Build.VERSION.SDK_INT >= 24) {
            mInfo.setDrawingOrder(drawingOrderInParent);
        }
    }

    /**
     * Gets the collection info if the node is a collection. A collection
     * child is always a collection item.
     *
     * @return The collection info.
     */
    public CollectionInfoCompat getCollectionInfo() {
        AccessibilityNodeInfo.CollectionInfo info = mInfo.getCollectionInfo();
        if (info != null) {
            return new CollectionInfoCompat(info);
        }
        return null;
    }

    public void setCollectionInfo(Object collectionInfo) {
        mInfo.setCollectionInfo((collectionInfo == null) ? null
                : (AccessibilityNodeInfo.CollectionInfo) ((CollectionInfoCompat)
                        collectionInfo).mInfo);

    }

    public void setCollectionItemInfo(Object collectionItemInfo) {
        mInfo.setCollectionItemInfo((collectionItemInfo == null) ? null
                : (AccessibilityNodeInfo.CollectionItemInfo) ((CollectionItemInfoCompat)
                        collectionItemInfo).mInfo);
    }

    /**
     * Gets the collection item info if the node is a collection item. A collection
     * item is always a child of a collection.
     *
     * @return The collection item info.
     */
    public CollectionItemInfoCompat getCollectionItemInfo() {
        AccessibilityNodeInfo.CollectionItemInfo info = mInfo.getCollectionItemInfo();
        if (info != null) {
            return new CollectionItemInfoCompat(info);
        }
        return null;
    }

    /**
     * Gets the range info if this node is a range.
     *
     * @return The range.
     */
    public RangeInfoCompat getRangeInfo() {
        AccessibilityNodeInfo.RangeInfo info = mInfo.getRangeInfo();
        if (info != null) {
            return new RangeInfoCompat(info);
        }
        return null;
    }

    /**
     * Sets the range info if this node is a range.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param rangeInfo The range info.
     */
    public void setRangeInfo(RangeInfoCompat rangeInfo) {
        mInfo.setRangeInfo((AccessibilityNodeInfo.RangeInfo) rangeInfo.mInfo);
    }

    /**
     * Gets the {@link android.view.accessibility.AccessibilityNodeInfo.ExtraRenderingInfo
     * extra rendering info} if the node is meant to be refreshed with extra data
     * to examine rendering related accessibility issues.
     *
     * @return The {@link android.view.accessibility.AccessibilityNodeInfo.ExtraRenderingInfo
     * extra rendering info}.
     */
    public AccessibilityNodeInfo.@Nullable ExtraRenderingInfo getExtraRenderingInfo() {
        if (Build.VERSION.SDK_INT >= 33) {
            return Api33Impl.getExtraRenderingInfo(mInfo);
        } else {
            return null;
        }
    }

    /**
     * Gets the {@link android.view.accessibility.AccessibilityNodeInfo#Selection selection} of this
     * node.
     *
     * @return The selection, or {@code null} if the node has no selection.
     *     <p>Compatibility:
     *     <ul>
     *       <li>API &lt: 36.1: Always returns {@code null}
     *     </ul>
     */
    @Nullable
    public SelectionCompat getSelection() {
        if (isAtLeastB_1()) {
            Selection selection = mInfo.getSelection();
            if (selection != null) {
                return new SelectionCompat(selection);
            }
        }

        return null;
    }

    /**
     * Gets the actions that can be performed on the node.
     *
     * @return A list of AccessibilityActions.
     */
    @SuppressWarnings({"unchecked", "MixedMutabilityReturnType"})
    public List<AccessibilityActionCompat> getActionList() {
        List<Object> actions = (List<Object>) (List<?>) mInfo.getActionList();
        List<AccessibilityActionCompat> result = new ArrayList<>();
        final int actionCount = actions.size();
        for (int i = 0; i < actionCount; i++) {
            Object action = actions.get(i);
            result.add(new AccessibilityActionCompat(action));
        }
        return result;
    }

    /**
     * Sets if the content of this node is invalid. For example,
     * a date is not well-formed.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param contentInvalid If the node content is invalid.
     */
    public void setContentInvalid(boolean contentInvalid) {
        mInfo.setContentInvalid(contentInvalid);
    }

    /**
     * Gets if the content of this node is invalid. For example,
     * a date is not well-formed.
     *
     * @return If the node content is invalid.
     */
    public boolean isContentInvalid() {
        return mInfo.isContentInvalid();
    }

    /**
     * Gets whether this node is context clickable.
     *
     * @return True if the node is context clickable.
     */
    public boolean isContextClickable() {
        return mInfo.isContextClickable();
    }

    /**
     * Sets whether this node is context clickable.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}. This class is made immutable
     * before being delivered to an AccessibilityService.
     * </p>
     *
     * @param contextClickable True if the node is context clickable.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setContextClickable(boolean contextClickable) {
        mInfo.setContextClickable(contextClickable);
    }

    /**
     * Gets the hint text of this node. Only applies to nodes where text can be entered.
     *
     * @return The hint text.
     */
    public @Nullable CharSequence getHintText() {
        if (Build.VERSION.SDK_INT >= 26) {
            return mInfo.getHintText();
        } else {
            return mInfo.getExtras().getCharSequence(HINT_TEXT_KEY);
        }
    }

    /**
     * Sets the hint text of this node. Only applies to nodes where text can be entered.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param hintText The hint text for this mode.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setHintText(@Nullable CharSequence hintText) {
        if (Build.VERSION.SDK_INT >= 26) {
            mInfo.setHintText(hintText);
        } else {
            mInfo.getExtras().putCharSequence(HINT_TEXT_KEY, hintText);
        }
    }


    /**
     * Sets the error text of this node.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param error The error text.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setError(CharSequence error) {
        mInfo.setError(error);
    }

    /**
     * Gets the error text of this node.
     *
     * @return The error text.
     */
    public CharSequence getError() {
        return mInfo.getError();
    }

    /**
     * Sets the view for which the view represented by this info serves as a
     * label for accessibility purposes.
     *
     * @param labeled The view for which this info serves as a label.
     */
    public void setLabelFor(View labeled) {
        mInfo.setLabelFor(labeled);
    }

    /**
     * Sets the view for which the view represented by this info serves as a
     * label for accessibility purposes. If <code>virtualDescendantId</code>
     * is {@link View#NO_ID} the root is set as the labeled.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report themselves as a tree of virtual views, thus conveying their
     * logical structure.
     * </p>
     *
     * @param root The root whose virtual descendant serves as a label.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void setLabelFor(View root, int virtualDescendantId) {
        mInfo.setLabelFor(root, virtualDescendantId);
    }

    /**
     * Gets the node info for which the view represented by this info serves as
     * a label for accessibility purposes.
     *
     * @return The labeled info.
     */
    public AccessibilityNodeInfoCompat getLabelFor() {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getLabelFor());
    }

    /**
     * Adds the view which serves as the label of the view represented by this info for
     * accessibility purposes. When multiple labels are added, the content from each label is
     * combined in the order that they are added.
     * <p>
     * If visible text can be used to describe or give meaning to this UI, this method is
     * preferred. For example, a TextView before an EditText in the UI usually specifies what
     * information is contained in the EditText. Hence, the EditText is labeled by the TextView.
     *
     * @param label A view that labels this node's source.
     */
    public void addLabeledBy(@NonNull View label) {
        addLabeledBy(label, NO_ID);
    }

    /**
     * Adds the view which serves as the label of the view represented by this info for
     * accessibility purposes. If <code>virtualDescendantId</code> is {@link View#NO_ID} the root
     * is set as the label.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view hierarchy
     * for accessibility purposes. This enables custom views that draw complex content to report
     * themselves as a tree of virtual views, thus conveying their logical structure.
     * <p>
     * If visible text can be used to describe or give meaning to this UI, this method is
     * preferred. For example, a TextView before an EditText in the UI usually specifies what
     * information is contained in the EditText. Hence, the EditText is labeled by the TextView.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * <p>
     *   <strong>Note:</strong>  Starting with Android 36, when multiple labels are added, the
     *   content from each label is combined in the order that they are added. Before Android 36,
     *   the most recently added label is set as the only label.
     * </p>
     *
     * @param root A root whose virtual descendant labels this node's source.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void addLabeledBy(@NonNull View root, int virtualDescendantId) {
        if (Build.VERSION.SDK_INT >= 36) {
            Api36Impl.addLabeledBy(mInfo, root, virtualDescendantId);
        } else {
            setLabeledBy(root, virtualDescendantId);
        }
    }

    /**
     * Gets the list of node infos which serve as the labels of the view represented by this info
     * for accessibility purposes.
     *
     * @return The list of labels in the order that they were added.
     */
    public @NonNull List<AccessibilityNodeInfoCompat> getLabeledByList() {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.getLabeledByList(mInfo);
        } else {
            List<AccessibilityNodeInfoCompat> labels = new ArrayList<>(1);
            AccessibilityNodeInfoCompat label = getLabeledBy();
            if (label != null) {
                labels.add(label);
            }
            return labels;
        }
    }

    /**
     * Removes a label. If the label was not previously added to the node, calling this method
     * has no effect.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * <p>
     *   <strong>Note:</strong> If the Android version is less than 36, this method has no effect;
     *   call {@link #setLabeledBy(View)} with <code>null</code> to remove the current label.
     * </p>
     *
     * @param label The node which serves as this node's label.
     * @return true if the label was present
     * @see #addLabeledBy(View)
     */
    public boolean removeLabeledBy(@NonNull View label) {
        return removeLabeledBy(label, NO_ID);
    }

    /**
     * Removes a label which is a virtual descendant of the given <code>root</code>. If
     * <code>virtualDescendantId</code> is {@link View#NO_ID} the root is set as the label. If
     * the label was not previously added to the node, calling this method has no effect.
     * <p>
     *   <strong>Note:</strong> If the Android version is less than 36, this method has no effect;
     *   call {@link #setLabeledBy(View)} with <code>null</code> to remove the current label.
     * </p>
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual node which serves as this node's label.
     * @return true if the label was present
     * @see #addLabeledBy(View, int)
     */
    public boolean removeLabeledBy(@NonNull View root, int virtualDescendantId) {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.removeLabeledBy(mInfo, root, virtualDescendantId);
        } else {
            return false;
        }
    }


    /**
     * Sets the view which serves as the label of the view represented by
     * this info for accessibility purposes.
     *
     * @param label The view that labels this node's source.
     * @deprecated Use {@link AccessibilityNodeInfoCompat#addLabeledBy(View)} or
     * {@link AccessibilityNodeInfoCompat#removeLabeledBy(View)} instead.
     */
    @Deprecated
    public void setLabeledBy(View label) {
        mInfo.setLabeledBy(label);
    }

    /**
     * Sets the view which serves as the label of the view represented by
     * this info for accessibility purposes. If <code>virtualDescendantId</code>
     * is {@link View#NO_ID} the root is set as the label.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report themselves as a tree of virtual views, thus conveying their
     * logical structure.
     * </p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param root The root whose virtual descendant labels this node's source.
     * @param virtualDescendantId The id of the virtual descendant.
     * @deprecated Use {@link AccessibilityNodeInfoCompat#addLabeledBy(View, int)} or
     * {@link AccessibilityNodeInfoCompat#removeLabeledBy(View, int)} instead.
     */
    @Deprecated
    public void setLabeledBy(View root, int virtualDescendantId) {
        mInfo.setLabeledBy(root, virtualDescendantId);
    }

    /**
     * Gets the node info which serves as the label of the view represented by
     * this info for accessibility purposes.
     *
     * @return The label.
     * @deprecated Use {@link AccessibilityNodeInfoCompat#getLabeledByList()} instead.
     */
    @Deprecated
    public AccessibilityNodeInfoCompat getLabeledBy() {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getLabeledBy());
    }

    /**
     * Gets if this node opens a popup or a dialog.
     *
     * @return If the the node opens a popup.
     */
    public boolean canOpenPopup() {
        return mInfo.canOpenPopup();
    }

    /**
     * Sets if this node opens a popup or a dialog.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param opensPopup If the the node opens a popup.
     */
    public void setCanOpenPopup(boolean opensPopup) {
        mInfo.setCanOpenPopup(opensPopup);
    }

    /**
     * Finds {@link AccessibilityNodeInfoCompat}s by the fully qualified view id's resource
     * name where a fully qualified id is of the from "package:id/id_resource_name".
     * For example, if the target application's package is "foo.bar" and the id
     * resource name is "baz", the fully qualified resource id is "foo.bar:id/baz".
     *
     * <p>
     *   <strong>Note:</strong> The primary usage of this API is for UI test automation
     *   and in order to report the fully qualified view id if an
     *   {@link AccessibilityNodeInfoCompat} the client has to set the
     *   {@link android.accessibilityservice.AccessibilityServiceInfo#FLAG_REPORT_VIEW_IDS}
     *   flag when configuring their {@link android.accessibilityservice.AccessibilityService}.
     * </p>
     *
     * @param viewId The fully qualified resource name of the view id to find.
     * @return A list of node info.
     */
    @SuppressWarnings("MixedMutabilityReturnType")
    public List<AccessibilityNodeInfoCompat> findAccessibilityNodeInfosByViewId(String viewId) {
        List<AccessibilityNodeInfo> nodes = mInfo.findAccessibilityNodeInfosByViewId(viewId);
        List<AccessibilityNodeInfoCompat> result = new ArrayList<>();
        for (AccessibilityNodeInfo node : nodes) {
                result.add(AccessibilityNodeInfoCompat.wrap(node));
            }
        return result;
    }

    /**
     * Gets an optional bundle with extra data. The bundle
     * is lazily created and never <code>null</code>.
     * <p>
     * <strong>Note:</strong> It is recommended to use the package
     * name of your application as a prefix for the keys to avoid
     * collisions which may confuse an accessibility service if the
     * same key has different meaning when emitted from different
     * applications.
     * </p>
     *
     * @return The bundle.
     */
    public Bundle getExtras() {
        return mInfo.getExtras();
    }

    /**
     * Gets the input type of the source as defined by {@link InputType}.
     *
     * @return The input type.
     */
    public int getInputType() {
        return mInfo.getInputType();
    }

    /**
     * Sets the input type of the source as defined by {@link InputType}.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an
     *   AccessibilityService.
     * </p>
     *
     * @param inputType The input type.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setInputType(int inputType) {
        mInfo.setInputType(inputType);
    }

    /**
     * Get the extra data available for this node.
     * <p>
     * Some data that is useful for some accessibility services is expensive to compute, and would
     * place undue overhead on apps to compute all the time. That data can be requested with
     * {@link android.view.accessibility.AccessibilityNodeInfo#refreshWithExtraData(String, Bundle)}.
     *
     * @return An unmodifiable list of keys corresponding to extra data that can be requested.
     * @see #EXTRA_DATA_RENDERING_INFO_KEY
     * @see #EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY
     */
    public @NonNull List<String> getAvailableExtraData() {
        if (Build.VERSION.SDK_INT >= 26) {
            return mInfo.getAvailableExtraData();
        } else {
            return emptyList();
        }
    }

    /**
     * Set the extra data available for this node.
     * <p>
     * <strong>Note:</strong> When a {@code View} passes in a non-empty list, it promises that
     * it will populate the node's extras with corresponding pieces of information in
     * {@link View#addExtraDataToAccessibilityNodeInfo(AccessibilityNodeInfo, String, Bundle)}.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}.
     * This class is made immutable before being delivered to an AccessibilityService.
     *
     * @param extraDataKeys A list of types of extra data that are available.
     * @see #getAvailableExtraData()
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setAvailableExtraData(@NonNull List<String> extraDataKeys) {
        if (Build.VERSION.SDK_INT >= 26) {
            mInfo.setAvailableExtraData(extraDataKeys);
        }
    }

    /**
     * Sets the maximum text length, or -1 for no limit.
     * <p>
     * Typically used to indicate that an editable text field has a limit on
     * the number of characters entered.
     * <p>
     * <strong>Note:</strong> Cannot be called from an
     * {@link android.accessibilityservice.AccessibilityService}.
     * This class is made immutable before being delivered to an AccessibilityService.
     *
     * @param max The maximum text length.
     * @see #getMaxTextLength()
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setMaxTextLength(int max) {
        mInfo.setMaxTextLength(max);
    }

    /**
     * Returns the maximum text length for this node.
     *
     * @return The maximum text length, or -1 for no limit.
     * @see #setMaxTextLength(int)
     */
    public int getMaxTextLength() {
        return mInfo.getMaxTextLength();
    }

    /**
     * Sets the text selection start and end.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param start The text selection start.
     * @param end The text selection end.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setTextSelection(int start, int end) {
        mInfo.setTextSelection(start, end);
    }

    /**
     * Gets the text selection start.
     *
     * @return The text selection start if there is selection or -1.
     */
    public int getTextSelectionStart() {
        return mInfo.getTextSelectionStart();
    }

    /**
     * Gets the text selection end.
     *
     * @return The text selection end if there is selection or -1.
     */
    public int getTextSelectionEnd() {
        return mInfo.getTextSelectionEnd();
    }

    /**
     * Gets the node before which this one is visited during traversal. A screen-reader
     * must visit the content of this node before the content of the one it precedes.
     *
     * @return The succeeding node if such or <code>null</code>.
     *
     * @see #setTraversalBefore(android.view.View)
     * @see #setTraversalBefore(android.view.View, int)
     */
    public AccessibilityNodeInfoCompat getTraversalBefore() {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getTraversalBefore());
    }

    /**
     * Sets the view before whose node this one should be visited during traversal. A
     * screen-reader must visit the content of this node before the content of the one
     * it precedes.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param view The view providing the preceding node.
     *
     * @see #getTraversalBefore()
     */
    public void setTraversalBefore(View view) {
        mInfo.setTraversalBefore(view);
    }

    /**
     * Sets the node before which this one is visited during traversal. A screen-reader
     * must visit the content of this node before the content of the one it precedes.
     * The successor is a virtual descendant of the given <code>root</code>. If
     * <code>virtualDescendantId</code> equals to {@link View#NO_ID} the root is set
     * as the successor.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report them selves as a tree of virtual views, thus conveying their
     * logical structure.
     * </p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void setTraversalBefore(View root, int virtualDescendantId) {
        mInfo.setTraversalBefore(root, virtualDescendantId);
    }

    /**
     * Gets the node after which this one is visited in accessibility traversal.
     * A screen-reader must visit the content of the other node before the content
     * of this one.
     *
     * @return The succeeding node if such or <code>null</code>.
     *
     * @see #setTraversalAfter(android.view.View)
     * @see #setTraversalAfter(android.view.View, int)
     */
    public AccessibilityNodeInfoCompat getTraversalAfter() {
        return AccessibilityNodeInfoCompat.wrapNonNullInstance(mInfo.getTraversalAfter());
    }

    /**
     * Sets the view whose node is visited after this one in accessibility traversal.
     * A screen-reader must visit the content of the other node before the content
     * of this one.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param view The previous view.
     *
     * @see #getTraversalAfter()
     */
    public void setTraversalAfter(View view) {
        mInfo.setTraversalAfter(view);
    }

    /**
     * Sets the node after which this one is visited in accessibility traversal.
     * A screen-reader must visit the content of the other node before the content
     * of this one. If <code>virtualDescendantId</code> equals to {@link View#NO_ID}
     * the root is set as the predecessor.
     * <p>
     * A virtual descendant is an imaginary View that is reported as a part of the view
     * hierarchy for accessibility purposes. This enables custom views that draw complex
     * content to report them selves as a tree of virtual views, thus conveying their
     * logical structure.
     * </p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param root The root of the virtual subtree.
     * @param virtualDescendantId The id of the virtual descendant.
     */
    public void setTraversalAfter(View root, int virtualDescendantId) {
        mInfo.setTraversalAfter(root, virtualDescendantId);
    }

    /**
     * Gets the window to which this node belongs.
     *
     * @return The window.
     *
     * @see android.accessibilityservice.AccessibilityService#getWindows()
     */
    public AccessibilityWindowInfoCompat getWindow() {
        return AccessibilityWindowInfoCompat.wrapNonNullInstance(mInfo.getWindow());
    }

    /**
     * Gets if the node can be dismissed.
     *
     * @return If the node can be dismissed.
     */
    public boolean isDismissable() {
        return mInfo.isDismissable();
    }

    /**
     * Sets if the node can be dismissed.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param dismissable If the node can be dismissed.
     */
    public void setDismissable(boolean dismissable) {
        mInfo.setDismissable(dismissable);
    }

    /**
     * Gets if the node is editable.
     *
     * @return True if the node is editable, false otherwise.
     */
    public boolean isEditable() {
        return mInfo.isEditable();
    }

    /**
     * Sets whether this node is editable.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param editable True if the node is editable.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setEditable(boolean editable) {
        mInfo.setEditable(editable);
    }

    /**
     * Gets if the node is a multi line editable text.
     *
     * @return True if the node is multi line.
     */
    public boolean isMultiLine() {
        return mInfo.isMultiLine();
    }

    /**
     * Sets if the node is a multi line editable text.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param multiLine True if the node is multi line.
     */
    public void setMultiLine(boolean multiLine) {
        mInfo.setMultiLine(multiLine);
    }

    /**
     * Gets the tooltip text of this node.
     *
     * @return The tooltip text.
     */
    public @Nullable CharSequence getTooltipText() {
        if (Build.VERSION.SDK_INT >= 28) {
            return mInfo.getTooltipText();
        } else {
            return mInfo.getExtras().getCharSequence(TOOLTIP_TEXT_KEY);
        }
    }

    /**
     * Sets the tooltip text of this node.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param tooltipText The tooltip text.
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setTooltipText(@Nullable CharSequence tooltipText) {
        if (Build.VERSION.SDK_INT >= 28) {
            mInfo.setTooltipText(tooltipText);
        } else {
            mInfo.getExtras().putCharSequence(TOOLTIP_TEXT_KEY, tooltipText);
        }
    }

    /**
     * If this node represents a visually distinct region of the screen that may update separately
     * from the rest of the window, it is considered a pane. Set the pane title to indicate that
     * the node is a pane, and to provide a title for it.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     * @param paneTitle The title of the window represented by this node.
     */
    public void setPaneTitle(@Nullable CharSequence paneTitle) {
        if (Build.VERSION.SDK_INT >= 28) {
            mInfo.setPaneTitle(paneTitle);
        } else {
            mInfo.getExtras().putCharSequence(PANE_TITLE_KEY, paneTitle);
        }
    }

    /**
     * Get the title of the pane represented by this node.
     *
     * @return The title of the pane represented by this node, or {@code null} if this node does
     *         not represent a pane.
     */
    public @Nullable CharSequence getPaneTitle() {
        if (Build.VERSION.SDK_INT >= 28) {
            return mInfo.getPaneTitle();
        } else {
            return mInfo.getExtras().getCharSequence(PANE_TITLE_KEY);
        }
    }

    /**
     * Returns whether the node is explicitly marked as a focusable unit by a screen reader. Note
     * that {@code false} indicates that it is not explicitly marked, not that the node is not
     * a focusable unit. Screen readers should generally use other signals, such as
     * {@link #isFocusable()}, or the presence of text in a node, to determine what should receive
     * focus.
     *
     * @return {@code true} if the node is specifically marked as a focusable unit for screen
     *         readers, {@code false} otherwise.
     */
    public boolean isScreenReaderFocusable() {
        if (Build.VERSION.SDK_INT >= 28) {
            return mInfo.isScreenReaderFocusable();
        }
        return getBooleanProperty(BOOLEAN_PROPERTY_SCREEN_READER_FOCUSABLE);
    }

    /**
     * Sets whether the node should be considered a focusable unit by a screen reader.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param screenReaderFocusable {@code true} if the node is a focusable unit for screen readers,
     *                              {@code false} otherwise.
     */
    public void setScreenReaderFocusable(boolean screenReaderFocusable) {
        if (Build.VERSION.SDK_INT >= 28) {
            mInfo.setScreenReaderFocusable(screenReaderFocusable);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_SCREEN_READER_FOCUSABLE, screenReaderFocusable);
        }
    }

    /**
     * Sets the {@link android.view.accessibility.AccessibilityNodeInfo#Selection selection} of this
     * node.
     *
     * <p><strong>Note:</strong> Cannot be called from an {@link
     * android.accessibilityservice.AccessibilityService}. This class is made immutable before being
     * delivered to an AccessibilityService.
     *
     * @param selection The selection, or {@code null} to clear the selection.
     *     <p>Compatibility:
     *     <ul>
     *       <li>API &lt: 36.1: Do nothing
     *     </ul>
     *
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setSelection(@Nullable SelectionCompat selection) {
        if (isAtLeastB_1()) {
            if (selection == null) {
                mInfo.setSelection(null);
            } else {
                mInfo.setSelection(selection.mSelection);
            }
        }
    }

    /**
     * Returns whether the node's text represents a hint for the user to enter text. It should only
     * be {@code true} if the node has editable text.
     *
     * @return {@code true} if the text in the node represents a hint to the user, {@code false}
     *     otherwise.
     */
    public boolean isShowingHintText() {
        if (Build.VERSION.SDK_INT >= 26) {
            return mInfo.isShowingHintText();
        }
        return getBooleanProperty(BOOLEAN_PROPERTY_IS_SHOWING_HINT);
    }

    /**
     * Sets whether the node's text represents a hint for the user to enter text. It should only
     * be {@code true} if the node has editable text.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param showingHintText {@code true} if the text in the node represents a hint to the user,
     * {@code false} otherwise.
     */
    public void setShowingHintText(boolean showingHintText) {
        if (Build.VERSION.SDK_INT >= 26) {
            mInfo.setShowingHintText(showingHintText);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_IS_SHOWING_HINT, showingHintText);
        }
    }

    /**
     * Returns whether node represents a heading.
     * <p><strong>Note:</strong> Returns {@code true} if either {@link #setHeading(boolean)}
     * marks this node as a heading or if the node has a {@link CollectionItemInfoCompat} that marks
     * it as such, to accommodate apps that use the now-deprecated API.</p>
     *
     * @return {@code true} if the node is a heading, {@code false} otherwise.
     */
    @SuppressWarnings("deprecation")
    public boolean isHeading() {
        if (Build.VERSION.SDK_INT >= 28) {
            return mInfo.isHeading();
        }
        if (getBooleanProperty(BOOLEAN_PROPERTY_IS_HEADING)) return true;
        CollectionItemInfoCompat collectionItemInfo = getCollectionItemInfo();
        return (collectionItemInfo != null) && collectionItemInfo.isHeading();
    }

    /**
     * Sets whether the node represents a heading.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param isHeading {@code true} if the node is a heading, {@code false} otherwise.
     */
    public void setHeading(boolean isHeading) {
        if (Build.VERSION.SDK_INT >= 28) {
            mInfo.setHeading(isHeading);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_IS_HEADING, isHeading);
        }
    }

    /**
     * Returns whether node represents a text entry key that is part of a keyboard or keypad.
     *
     * @return {@code true} if the node is a text entry key, {@code false} otherwise.
     */
    public boolean isTextEntryKey() {
        if (Build.VERSION.SDK_INT >= 29) {
            return mInfo.isTextEntryKey();
        }
        return getBooleanProperty(BOOLEAN_PROPERTY_IS_TEXT_ENTRY_KEY);
    }

    /**
     * Sets whether the node represents a text entry key that is part of a keyboard or keypad.
     * <p>This method has no effect below API 19</p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param isTextEntryKey {@code true} if the node is a text entry key, {@code false} otherwise.
     */
    public void setTextEntryKey(boolean isTextEntryKey) {
        if (Build.VERSION.SDK_INT >= 29) {
            mInfo.setTextEntryKey(isTextEntryKey);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_IS_TEXT_ENTRY_KEY, isTextEntryKey);
        }
    }

    /**
     * Gets whether the node has {@link #setRequestInitialAccessibilityFocus}.
     *
     * @return True if the node has requested initial accessibility focus.
     */
    @SuppressLint("KotlinPropertyAccess")
    public boolean hasRequestInitialAccessibilityFocus() {
        if (Build.VERSION.SDK_INT >= 34) {
            return Api34Impl.hasRequestInitialAccessibilityFocus(mInfo);
        } else {
            return getBooleanProperty(BOOLEAN_PROPERTY_HAS_REQUEST_INITIAL_ACCESSIBILITY_FOCUS);
        }
    }

    /**
     * Sets whether the node has requested initial accessibility focus.
     *
     * <p>
     * If the node {@link #hasRequestInitialAccessibilityFocus}, this node would be one of
     * candidates to be accessibility focused when the window appears.
     * </p>
     *
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param requestInitialAccessibilityFocus True if the node requests to receive initial
     *                                         accessibility focus.
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    @SuppressLint("GetterSetterNames")
    public void setRequestInitialAccessibilityFocus(boolean requestInitialAccessibilityFocus) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setRequestInitialAccessibilityFocus(mInfo, requestInitialAccessibilityFocus);
        } else {
            setBooleanProperty(BOOLEAN_PROPERTY_HAS_REQUEST_INITIAL_ACCESSIBILITY_FOCUS,
                    requestInitialAccessibilityFocus);
        }
    }

    /**
     * Refreshes this info with the latest state of the view it represents.
     * <p>
     * <strong>Note:</strong> If this method returns false this info is obsolete
     * since it represents a view that is no longer in the view tree.
     * </p>
     * @return Whether the refresh succeeded.
     */
    public boolean refresh() {
        return mInfo.refresh();
    }

    /**
     * Gets the custom role description.
     * @return The role description.
     */
    public @Nullable CharSequence getRoleDescription() {
        return mInfo.getExtras().getCharSequence(ROLE_DESCRIPTION_KEY);
    }

    /**
     * Sets the custom role description.
     *
     * <p>
     *   The role description allows you to customize the name for the view's semantic
     *   role. For example, if you create a custom subclass of {@link android.view.View}
     *   to display a menu bar, you could assign it the role description of "menu bar".
     * </p>
     * <p>
     *   <strong>Warning:</strong> For consistency with other applications, you should
     *   not use the role description to force accessibility services to describe
     *   standard views (such as buttons or checkboxes) using specific wording. For
     *   example, you should not set a role description of "check box" or "tick box" for
     *   a standard {@link android.widget.CheckBox}. Instead let accessibility services
     *   decide what feedback to provide.
     * </p>
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an AccessibilityService.
     * </p>
     *
     * @param roleDescription The role description.
     */
    public void setRoleDescription(@Nullable CharSequence roleDescription) {
        mInfo.getExtras().putCharSequence(ROLE_DESCRIPTION_KEY, roleDescription);
    }

    /**
     * Get the {@link TouchDelegateInfoCompat} for touch delegate behavior with the represented
     * view. It is possible for the same node to be pointed to by several regions. Use
     * {@link TouchDelegateInfoCompat#getRegionAt(int)} to get touch delegate target
     * {@link Region}, and {@link TouchDelegateInfoCompat#getTargetForRegion(Region)}
     * for {@link AccessibilityNodeInfoCompat} from the given region.
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 29: Always returns {@code null}</li>
     * </ul>
     *
     * @return {@link TouchDelegateInfoCompat} or {@code null} if there are no touch delegates
     * in this node.
     */
    public @Nullable TouchDelegateInfoCompat getTouchDelegateInfo() {
        if (Build.VERSION.SDK_INT >= 29) {
            TouchDelegateInfo delegateInfo = mInfo.getTouchDelegateInfo();
            if (delegateInfo != null) {
                return new TouchDelegateInfoCompat(delegateInfo);
            }
        }
        return null;
    }

    /**
     * Set touch delegate info if the represented view has a {@link android.view.TouchDelegate}.
     * <p>
     *   <strong>Note:</strong> Cannot be called from an
     *   {@link android.accessibilityservice.AccessibilityService}.
     *   This class is made immutable before being delivered to an
     *   AccessibilityService.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 29: No-op</li>
     * </ul>
     *
     * @param delegatedInfo {@link TouchDelegateInfoCompat}
     * @throws IllegalStateException If called from an AccessibilityService.
     */
    public void setTouchDelegateInfo(@NonNull TouchDelegateInfoCompat delegatedInfo) {
        if (Build.VERSION.SDK_INT >= 29) {
            mInfo.setTouchDelegateInfo(delegatedInfo.mInfo);
        }
    }

    /**
     * Connects this node to the View's root so that operations on this node can query the entire
     * {@link AccessibilityNodeInfoCompat} tree and perform accessibility actions on nodes.
     *
     * <p>
     * Testing or debugging tools should create this {@link AccessibilityNodeInfoCompat} node using
     * {@link ViewCompat#onInitializeAccessibilityNodeInfo(View, AccessibilityNodeInfoCompat)}
     * or {@link AccessibilityNodeProviderCompat} and call this
     * method, then navigate and interact with the node tree by calling methods on the node.
     * Calling this method more than once on the same node is a no-op. After calling this method,
     * all nodes linked to this node (children, ancestors, etc.) are also queryable.
     * </p>
     *
     * <p>
     * Here "query" refers to the following node operations:
     * <ul>
     *      <li>check properties of this node (example: {@link #isScrollable()})</li>
     *      <li>find and query children (example: {@link #getChild(int)})</li>
     *      <li>find and query the parent (example: {@link #getParent()})</li>
     *      <li>find focus (examples: {@link #findFocus(int)}, {@link #focusSearch(int)})</li>
     *      <li>find and query other nodes (example:
     *      {@link #findAccessibilityNodeInfosByText(String)},
     *      {@link #findAccessibilityNodeInfosByViewId(String)})</li>
     *      <li>perform actions (example: {@link #performAction(int)})</li>
     * </ul>
     * </p>
     *
     * <p>
     * This is intended for short-lived inspections from testing or debugging tools in the app
     * process, as operations on this node tree will only succeed as long as the associated
     * view hierarchy remains attached to a window. {@link AccessibilityNodeInfoCompat} objects can
     * quickly become out of sync with their corresponding {@link View} objects; if you wish to
     * inspect a changed or different view hierarchy then create a new node from any view in that
     * hierarchy and call this method on that new node, instead of disabling & re-enabling the
     * connection on the previous node.
     * </p>
     * <p>
     * Compatibility:
     * <ul>
     *     <li>API &lt; 34: No-op</li>
     * </ul>
     *
     * @param view The view that generated this node, or any view in the same view-root hierarchy.
     * @param enabled Whether to enable (true) or disable (false) querying from the app process.
     * @throws IllegalStateException If called from an {@link AccessibilityService}, or if provided
     *                               a {@link View} that is not attached to a window.
     */
    public void setQueryFromAppProcessEnabled(@NonNull View view, boolean enabled) {
        if (Build.VERSION.SDK_INT >= 34) {
            Api34Impl.setQueryFromAppProcessEnabled(mInfo, view, enabled);
        }
    }

    /**
     * Returns the supplemental description of this {@link AccessibilityNodeInfoCompat}.
     * <p>
     * A supplemental description provides brief supplemental information for this node, such as
     * the purpose of the node when that purpose is not conveyed within its textual representation.
     * For example, if a dropdown select has a purpose of setting font family, the supplemental
     * description could be "font family". If this node has children, its supplemental description
     * serves as additional information and is not intended to replace any existing information in
     * the subtree. This is different from the {@link #getContentDescription()} in that this
     * description is purely supplemental while a content description may be used to replace a
     * description for a node or its subtree that an assistive technology would otherwise compute
     * based on other properties of the node and its descendants.
     *
     * @return The supplemental description.
     * @see #setSupplementalDescription(CharSequence)
     * @see #getContentDescription()
     */
    @Nullable
    public CharSequence getSupplementalDescription() {
        if (Build.VERSION.SDK_INT >= 36) {
            return Api36Impl.getSupplementalDescription(mInfo);
        } else {
            return mInfo.getExtras().getCharSequence(SUPPLEMENTAL_DESCRIPTION_KEY);
        }
    }

    /**
     * Sets the supplemental description of this {@link AccessibilityNodeInfoCompat}.
     * <p>
     * A supplemental description provides brief supplemental information for this node, such as
     * the purpose of the node when that purpose is not conveyed within its textual representation.
     * For example, if a dropdown select has a purpose of setting font family, the supplemental
     * description could be "font family". If this node has children, its supplemental description
     * serves as additional information and is not intended to replace any existing information in
     * the subtree. This is different from the {@link #setContentDescription(CharSequence)} in that
     * this description is purely supplemental while a content description may be used to replace a
     * description for a node or its subtree that an assistive technology would otherwise compute
     * based on other properties of the node and its descendants.
     *
     * <p>
     * <strong>Note:</strong> Cannot be called from an {@link
     * android.accessibilityservice.AccessibilityService}. This class is made immutable before being
     * delivered to an AccessibilityService.
     *
     * @param supplementalDescription The supplemental description.
     * @throws IllegalStateException If called from an AccessibilityService.
     * @see #getSupplementalDescription()
     * @see #setContentDescription(CharSequence)
     */
    public void setSupplementalDescription(@Nullable CharSequence supplementalDescription) {
        if (Build.VERSION.SDK_INT >= 36) {
            Api36Impl.setSupplementalDescription(mInfo, supplementalDescription);
        } else {
            mInfo.getExtras()
                    .putCharSequence(SUPPLEMENTAL_DESCRIPTION_KEY, supplementalDescription);
        }
    }

    @Override
    public int hashCode() {
        return (mInfo == null) ? 0 : mInfo.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (!(obj instanceof AccessibilityNodeInfoCompat)) {
            return false;
        }
        AccessibilityNodeInfoCompat other = (AccessibilityNodeInfoCompat) obj;
        if (mInfo == null) {
            if (other.mInfo != null) {
                return false;
            }
        } else if (!mInfo.equals(other.mInfo)) {
            return false;
        }
        if (mVirtualDescendantId != other.mVirtualDescendantId) {
            return false;
        }
        if (mParentVirtualDescendantId != other.mParentVirtualDescendantId) {
            return false;
        }
        return true;
    }

    @SuppressWarnings("deprecation")
    @Override
    public @NonNull String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append(super.toString());

        Rect bounds = new Rect();

        getBoundsInParent(bounds);
        builder.append("; boundsInParent: " + bounds);

        getBoundsInScreen(bounds);
        builder.append("; boundsInScreen: " + bounds);

        getBoundsInWindow(bounds);
        builder.append("; boundsInWindow: " + bounds);

        builder.append("; packageName: ").append(getPackageName());
        builder.append("; className: ").append(getClassName());
        builder.append("; text: ").append(getText());
        builder.append("; error: ").append(getError());
        builder.append("; maxTextLength: ").append(getMaxTextLength());
        builder.append("; stateDescription: ").append(getStateDescription());
        builder.append("; contentDescription: ").append(getContentDescription());
        builder.append("; supplementalDescription: ").append(getSupplementalDescription());
        builder.append("; tooltipText: ").append(getTooltipText());
        builder.append("; viewIdResName: ").append(getViewIdResourceName());
        builder.append("; uniqueId: ").append(getUniqueId());

        builder.append("; checkable: ").append(isCheckable());
        builder.append("; checked: ").append(getCheckedString());
        builder.append("; fieldRequired: ").append(isFieldRequired());
        builder.append("; focusable: ").append(isFocusable());
        builder.append("; focused: ").append(isFocused());
        builder.append("; selected: ").append(isSelected());
        builder.append("; clickable: ").append(isClickable());
        builder.append("; longClickable: ").append(isLongClickable());
        builder.append("; contextClickable: ").append(isContextClickable());
        builder.append("; expandedState: ").append(
                getExpandedStateSymbolicName(getExpandedState()));
        builder.append("; enabled: ").append(isEnabled());
        builder.append("; password: ").append(isPassword());
        builder.append("; scrollable: " + isScrollable());
        builder.append("; containerTitle: ").append(getContainerTitle());
        builder.append("; granularScrollingSupported: ").append(isGranularScrollingSupported());
        builder.append("; importantForAccessibility: ").append(isImportantForAccessibility());
        builder.append("; visible: ").append(isVisibleToUser());
        builder.append("; isTextSelectable: ").append(isTextSelectable());
        builder.append("; accessibilityDataSensitive: ").append(isAccessibilityDataSensitive());

        builder.append("; [");
        List<AccessibilityActionCompat> actions = getActionList();
        for (int i = 0; i < actions.size(); i++) {
            AccessibilityActionCompat action = actions.get(i);
            String actionName = getActionSymbolicName(action.getId());
            if (actionName.equals("ACTION_UNKNOWN") && action.getLabel() != null) {
                actionName = action.getLabel().toString();
            }
            builder.append(actionName);
            if (i != actions.size() - 1) {
                builder.append(", ");
            }
        }
        builder.append("]");

        return builder.toString();
    }

    private void setBooleanProperty(int property, boolean value) {
        Bundle extras = getExtras();
        if (extras != null) {
            int booleanProperties = extras.getInt(BOOLEAN_PROPERTY_KEY, 0);
            booleanProperties &= ~property;
            booleanProperties |= value ? property : 0;
            extras.putInt(BOOLEAN_PROPERTY_KEY, booleanProperties);
        }
    }

    private boolean getBooleanProperty(int property) {
        Bundle extras = getExtras();
        if (extras == null) return false;
        return (extras.getInt(BOOLEAN_PROPERTY_KEY, 0) & property) == property;
    }

    private String getCheckedString() {
        @CheckedState int checkedState = getChecked();
        if (checkedState == CHECKED_STATE_TRUE) {
            return "TRUE";
        } else if (checkedState == CHECKED_STATE_PARTIAL) {
            return "PARTIAL";
        } else {
            return "FALSE";
        }
    }

    static String getActionSymbolicName(int action) {
        switch (action) {
            case ACTION_FOCUS:
                return "ACTION_FOCUS";
            case ACTION_CLEAR_FOCUS:
                return "ACTION_CLEAR_FOCUS";
            case ACTION_SELECT:
                return "ACTION_SELECT";
            case ACTION_CLEAR_SELECTION:
                return "ACTION_CLEAR_SELECTION";
            case ACTION_CLICK:
                return "ACTION_CLICK";
            case ACTION_LONG_CLICK:
                return "ACTION_LONG_CLICK";
            case ACTION_ACCESSIBILITY_FOCUS:
                return "ACTION_ACCESSIBILITY_FOCUS";
            case ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                return "ACTION_CLEAR_ACCESSIBILITY_FOCUS";
            case ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
                return "ACTION_NEXT_AT_MOVEMENT_GRANULARITY";
            case ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
                return "ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY";
            case ACTION_NEXT_HTML_ELEMENT:
                return "ACTION_NEXT_HTML_ELEMENT";
            case ACTION_PREVIOUS_HTML_ELEMENT:
                return "ACTION_PREVIOUS_HTML_ELEMENT";
            case ACTION_SCROLL_FORWARD:
                return "ACTION_SCROLL_FORWARD";
            case ACTION_SCROLL_BACKWARD:
                return "ACTION_SCROLL_BACKWARD";
            case ACTION_CUT:
                return "ACTION_CUT";
            case ACTION_COPY:
                return "ACTION_COPY";
            case ACTION_PASTE:
                return "ACTION_PASTE";
            case ACTION_SET_SELECTION:
                return "ACTION_SET_SELECTION";
            case ACTION_EXPAND:
                return "ACTION_EXPAND";
            case ACTION_COLLAPSE:
                return "ACTION_COLLAPSE";
            case ACTION_SET_TEXT:
                return "ACTION_SET_TEXT";
            case android.R.id.accessibilityActionScrollUp:
                return "ACTION_SCROLL_UP";
            case android.R.id.accessibilityActionScrollLeft:
                return "ACTION_SCROLL_LEFT";
            case android.R.id.accessibilityActionScrollDown:
                return "ACTION_SCROLL_DOWN";
            case android.R.id.accessibilityActionScrollRight:
                return "ACTION_SCROLL_RIGHT";
            case android.R.id.accessibilityActionPageDown:
                return "ACTION_PAGE_DOWN";
            case android.R.id.accessibilityActionPageUp:
                return "ACTION_PAGE_UP";
            case android.R.id.accessibilityActionPageLeft:
                return "ACTION_PAGE_LEFT";
            case android.R.id.accessibilityActionPageRight:
                return "ACTION_PAGE_RIGHT";
            case android.R.id.accessibilityActionShowOnScreen:
                return "ACTION_SHOW_ON_SCREEN";
            case android.R.id.accessibilityActionScrollToPosition:
                return "ACTION_SCROLL_TO_POSITION";
            case android.R.id.accessibilityActionContextClick:
                return "ACTION_CONTEXT_CLICK";
            case android.R.id.accessibilityActionSetProgress:
                return "ACTION_SET_PROGRESS";
            case android.R.id.accessibilityActionMoveWindow:
                return "ACTION_MOVE_WINDOW";
            case android.R.id.accessibilityActionShowTooltip:
                return "ACTION_SHOW_TOOLTIP";
            case android.R.id.accessibilityActionHideTooltip:
                return "ACTION_HIDE_TOOLTIP";
            case android.R.id.accessibilityActionPressAndHold:
                return "ACTION_PRESS_AND_HOLD";
            case android.R.id.accessibilityActionImeEnter:
                return "ACTION_IME_ENTER";
            case android.R.id.accessibilityActionDragStart:
                return "ACTION_DRAG_START";
            case android.R.id.accessibilityActionDragDrop:
                return "ACTION_DRAG_DROP";
            case android.R.id.accessibilityActionDragCancel:
                return "ACTION_DRAG_CANCEL";
            case android.R.id.accessibilityActionScrollInDirection:
                return "ACTION_SCROLL_IN_DIRECTION";
            case android.R.id.accessibilityActionSetExtendedSelection:
                return "ACTION_SET_EXTENDED_SELECTION";
            default:
                return "ACTION_UNKNOWN";
        }
    }

    static String getExpandedStateSymbolicName(@ExpandedState int state) {
        switch (state) {
            case EXPANDED_STATE_UNDEFINED:
                return "UNDEFINED";
            case EXPANDED_STATE_COLLAPSED:
                return "COLLAPSED";
            case EXPANDED_STATE_PARTIAL:
                return "PARTIAL";
            case EXPANDED_STATE_FULL:
                return "FULL";
            default:
                return "UNKNOWN";
        }
    }

    @RequiresApi(30)
    private static class Api30Impl {
        private Api30Impl() {
            // This class is non instantiable.
        }

        public static void setStateDescription(AccessibilityNodeInfo info,
                CharSequence stateDescription) {
            info.setStateDescription(stateDescription);
        }

        public static CharSequence getStateDescription(AccessibilityNodeInfo info) {
            return info.getStateDescription();
        }

        public static Object createRangeInfo(int type, float min, float max, float current) {
            return new AccessibilityNodeInfo.RangeInfo(type, min, max, current);
        }
    }

    @RequiresApi(33)
    private static class Api33Impl {
        private Api33Impl() {
            // This class is non instantiable.
        }

        public static AccessibilityNodeInfo.ExtraRenderingInfo getExtraRenderingInfo(
                AccessibilityNodeInfo info) {
            return info.getExtraRenderingInfo();
        }

        public static boolean isTextSelectable(AccessibilityNodeInfo info) {
            return info.isTextSelectable();
        }

        public static void setTextSelectable(AccessibilityNodeInfo info, boolean selectable) {
            info.setTextSelectable(selectable);
        }

        public static CollectionItemInfoCompat buildCollectionItemInfoCompat(
                boolean heading, int columnIndex, int rowIndex, int columnSpan,
                int rowSpan, boolean selected, String rowTitle, String columnTitle) {
            return new CollectionItemInfoCompat(
                    new AccessibilityNodeInfo.CollectionItemInfo.Builder()
                    .setHeading(heading).setColumnIndex(columnIndex)
                    .setRowIndex(rowIndex)
                    .setColumnSpan(columnSpan)
                    .setRowSpan(rowSpan)
                    .setSelected(selected)
                    .setRowTitle(rowTitle)
                    .setColumnTitle(columnTitle)
                    .build());
        }

        public static AccessibilityNodeInfoCompat getChild(AccessibilityNodeInfo info, int index,
                int prefetchingStrategy) {
            return AccessibilityNodeInfoCompat.wrapNonNullInstance(info.getChild(index,
                    prefetchingStrategy));
        }

        public static AccessibilityNodeInfoCompat getParent(AccessibilityNodeInfo info,
                int prefetchingStrategy) {
            return AccessibilityNodeInfoCompat.wrapNonNullInstance(info.getParent(
                    prefetchingStrategy));
        }

        public static String getUniqueId(AccessibilityNodeInfo info) {
            return info.getUniqueId();
        }

        public static void setUniqueId(AccessibilityNodeInfo info, String uniqueId) {
            info.setUniqueId(uniqueId);
        }

        public static String getCollectionItemRowTitle(Object info) {
            return ((AccessibilityNodeInfo.CollectionItemInfo) info).getRowTitle();

        }

        public static String getCollectionItemColumnTitle(Object info) {
            return ((AccessibilityNodeInfo.CollectionItemInfo) info).getColumnTitle();
        }
    }

    @RequiresApi(34)
    private static class Api34Impl {
        private Api34Impl() {
            // This class is non instantiable.
        }

        public static boolean isAccessibilityDataSensitive(AccessibilityNodeInfo info) {
            return info.isAccessibilityDataSensitive();
        }

        public static void setAccessibilityDataSensitive(AccessibilityNodeInfo info,
                boolean accessibilityDataSensitive) {
            info.setAccessibilityDataSensitive(accessibilityDataSensitive);
        }

        public static CharSequence getContainerTitle(AccessibilityNodeInfo info) {
            return info.getContainerTitle();
        }

        public static void setContainerTitle(AccessibilityNodeInfo info,
                CharSequence containerTitle) {
            info.setContainerTitle(containerTitle);
        }

        public static void getBoundsInWindow(AccessibilityNodeInfo info, Rect bounds) {
            info.getBoundsInWindow(bounds);
        }

        public static void setBoundsInWindow(AccessibilityNodeInfo info, Rect bounds) {
            info.setBoundsInWindow(bounds);
        }

        public static boolean hasRequestInitialAccessibilityFocus(AccessibilityNodeInfo info) {
            return info.hasRequestInitialAccessibilityFocus();
        }

        public static void setRequestInitialAccessibilityFocus(AccessibilityNodeInfo info,
                boolean requestInitialAccessibilityFocus) {
            info.setRequestInitialAccessibilityFocus(requestInitialAccessibilityFocus);
        }

        public static long getMinDurationBetweenContentChangeMillis(AccessibilityNodeInfo info) {
            return info.getMinDurationBetweenContentChanges().toMillis();
        }

        public static void setMinDurationBetweenContentChangeMillis(AccessibilityNodeInfo info,
                long duration) {
            info.setMinDurationBetweenContentChanges(Duration.ofMillis(duration));
        }

        public static void setQueryFromAppProcessEnabled(AccessibilityNodeInfo info, View view,
                boolean enabled) {
            info.setQueryFromAppProcessEnabled(view, enabled);
        }

        public static AccessibilityNodeInfo.AccessibilityAction getActionScrollInDirection() {
            return AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_IN_DIRECTION;
        }
    }

    @RequiresApi(35)
    private static class Api35Impl {
        private Api35Impl() {
            // This class is non instantiable.
        }

        public static int getItemCount(Object info) {
            return ((AccessibilityNodeInfo.CollectionInfo) info).getItemCount();
        }

        public static int getImportantForAccessibilityItemCount(Object info) {
            return ((AccessibilityNodeInfo.CollectionInfo) info)
                    .getImportantForAccessibilityItemCount();
        }

        public static CollectionInfoCompat buildCollectionInfoCompat(int rowCount, int columnCount,
                boolean hierarchical, int selectionMode, int itemCount,
                int importantForAccessibilityItemCount) {
            return new CollectionInfoCompat(new AccessibilityNodeInfo.CollectionInfo.Builder()
                    .setRowCount(rowCount)
                    .setColumnCount(columnCount)
                    .setHierarchical(hierarchical)
                    .setSelectionMode(selectionMode)
                    .setItemCount(itemCount)
                    .setImportantForAccessibilityItemCount(importantForAccessibilityItemCount)
                    .build());
        }
    }

    @RequiresApi(36)
    private static class Api36Impl {
        private Api36Impl() {
            // This class is non instantiable.
        }

        @ExpandedState
        public static int getExpandedState(AccessibilityNodeInfo info) {
            return info.getExpandedState();
        }

        public static void setExpandedState(AccessibilityNodeInfo info, @ExpandedState int state) {
            info.setExpandedState(state);
        }

        public static boolean isFieldRequired(AccessibilityNodeInfo info) {
            return info.isFieldRequired();
        }

        public static void setFieldRequired(AccessibilityNodeInfo info, boolean required) {
            info.setFieldRequired(required);
        }

        @Nullable
        public static CharSequence getSupplementalDescription(AccessibilityNodeInfo info) {
            return info.getSupplementalDescription();
        }

        public static void setSupplementalDescription(
                AccessibilityNodeInfo info, @Nullable CharSequence supplementalDescription) {
            info.setSupplementalDescription(supplementalDescription);
        }

        @CheckedState
        private static int getChecked(AccessibilityNodeInfo info) {
            return info.getChecked();
        }

        private static void setChecked(AccessibilityNodeInfo info, @CheckedState int checked) {
            info.setChecked(checked);
        }

        private static void addLabeledBy(AccessibilityNodeInfo info, @NonNull View root,
                int virtualDescendantId) {
            info.addLabeledBy(root, virtualDescendantId);
        }

        private static @NonNull List<AccessibilityNodeInfoCompat> getLabeledByList(
                AccessibilityNodeInfo info) {
            List<AccessibilityNodeInfo> labels = info.getLabeledByList();
            List<AccessibilityNodeInfoCompat> compatLabels = new ArrayList<>(labels.size());
            for (AccessibilityNodeInfo labeledByInfo : labels) {
                compatLabels.add(AccessibilityNodeInfoCompat.wrap(labeledByInfo));
            }
            return compatLabels;
        }

        private static boolean removeLabeledBy(AccessibilityNodeInfo info, @NonNull View root,
                int virtualDescendantId) {
            return info.removeLabeledBy(root, virtualDescendantId);
        }
    }

    @RequiresApi(Build.VERSION_CODES_FULL.BAKLAVA_1)
    private static class Api36MinorImpl {
        private Api36MinorImpl() {
            // This class is non instantiable.
        }

        public static CollectionItemInfoCompat buildCollectionItemInfoCompat(
                boolean heading,
                int columnIndex,
                int rowIndex,
                int columnSpan,
                int rowSpan,
                boolean selected,
                String rowTitle,
                String columnTitle,
                @CollectionItemInfoCompat.SortDirection int sortDirection) {
            return new CollectionItemInfoCompat(
                    new AccessibilityNodeInfo.CollectionItemInfo.Builder()
                            .setHeading(heading)
                            .setColumnIndex(columnIndex)
                            .setRowIndex(rowIndex)
                            .setColumnSpan(columnSpan)
                            .setRowSpan(rowSpan)
                            .setSelected(selected)
                            .setRowTitle(rowTitle)
                            .setColumnTitle(columnTitle)
                            .setSortDirection(sortDirection)
                            .build());
        }

        public static @CollectionItemInfoCompat.SortDirection int getCollectionItemSortDirection(
                Object info) {
            return ((AccessibilityNodeInfo.CollectionItemInfo) info).getSortDirection();
        }

        public static AccessibilityNodeInfo.AccessibilityAction getActionSetExtendedSelection() {
            return AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_EXTENDED_SELECTION;
        }
    }
}
