// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.text.SpannableString;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.content.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * Popup window that displays a menu for viewing and applying text replacement suggestions.
 */
public abstract class SuggestionsPopupWindow
        implements OnItemClickListener, OnDismissListener, View.OnClickListener {
    private static final String ACTION_USER_DICTIONARY_INSERT =
            "com.android.settings.USER_DICTIONARY_INSERT";
    private static final String USER_DICTIONARY_EXTRA_WORD = "word";

    // From Android Settings app's @integer/maximum_user_dictionary_word_length.
    private static final int ADD_TO_DICTIONARY_MAX_LENGTH_ON_JELLY_BEAN = 48;

    private final Context mContext;
    protected final TextSuggestionHost mTextSuggestionHost;
    private final View mParentView;
    private WindowAndroid mWindowAndroid;

    private Activity mActivity;
    private DisplayMetrics mDisplayMetrics;
    private PopupWindow mPopupWindow;
    private LinearLayout mContentView;

    private String mHighlightedText;
    private int mNumberOfSuggestionsToUse;
    private TextView mAddToDictionaryButton;
    private TextView mDeleteButton;
    private ListView mSuggestionListView;
    private LinearLayout mListFooter;
    private View mDivider;
    private int mPopupVerticalMargin;

    private boolean mDismissedByItemTap;
    /**
     * @param context Android context to use.
     * @param textSuggestionHost TextSuggestionHost instance (used to communicate with Blink).
     * @param windowAndroid The current WindowAndroid instance.
     * @param parentView The view used to attach the PopupWindow.
     */
    public SuggestionsPopupWindow(Context context, TextSuggestionHost textSuggestionHost,
            WindowAndroid windowAndroid, View parentView) {
        mContext = context;
        mTextSuggestionHost = textSuggestionHost;
        mWindowAndroid = windowAndroid;
        mParentView = parentView;

        createPopupWindow();
        initContentView();

        mPopupWindow.setContentView(mContentView);
    }

    /**
     * Method to be implemented by subclasses that returns how mnay suggestions are available (some
     * of them may not be displayed if there's not enough room in the window).
     */
    protected abstract int getSuggestionsCount();

    /**
     * Method to be implemented by subclasses to return an object representing the suggestion at
     * the specified position.
     */
    protected abstract Object getSuggestionItem(int position);

    /**
     * Method to be implemented by subclasses to return a SpannableString representing text,
     * possibly with formatting added, to display for the suggestion at the specified position.
     */
    protected abstract SpannableString getSuggestionText(int position);

    /**
     * Method to be implemented by subclasses to apply the suggestion at the specified position.
     */
    protected abstract void applySuggestion(int position);

    /**
     * Hides or shows the "Add to dictionary" button in the suggestion menu footer.
     */
    protected void setAddToDictionaryEnabled(boolean isEnabled) {
        mAddToDictionaryButton.setVisibility(isEnabled ? View.VISIBLE : View.GONE);
    }

    private void createPopupWindow() {
        mPopupWindow = new PopupWindow();
        mPopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Set the background on the PopupWindow instead of on mContentView (where we set it for
            // pre-Lollipop) since the popup will not properly dismiss on pre-Marshmallow unless it
            // has a background set.
            mPopupWindow.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                    mContext.getResources(), R.drawable.floating_popup_background_light));
            // On Lollipop and later, we use elevation to create a drop shadow effect.
            // On pre-Lollipop, we instead use a background image on mContentView (in
            // initContentView).
            mPopupWindow.setElevation(mContext.getResources().getDimensionPixelSize(
                    R.dimen.text_suggestion_popup_elevation));
        } else {
            // The PopupWindow does not properly dismiss pre-Marshmallow unless it has a background
            // set.
            mPopupWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        mPopupWindow.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        mPopupWindow.setFocusable(true);
        mPopupWindow.setClippingEnabled(false);
        mPopupWindow.setOnDismissListener(this);
    }

    private void initContentView() {
        final LayoutInflater inflater =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mContentView =
                (LinearLayout) inflater.inflate(R.layout.text_edit_suggestion_container, null);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            // Set this on the content view instead of on the PopupWindow so we can retrieve the
            // padding later.
            mContentView.setBackground(ApiCompatibilityUtils.getDrawable(
                    mContext.getResources(), R.drawable.popup_bg));
        }

        // mPopupVerticalMargin is the minimum amount of space we want to have between the popup
        // and the top or bottom of the window.
        mPopupVerticalMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.text_suggestion_popup_vertical_margin);

        mSuggestionListView = (ListView) mContentView.findViewById(R.id.suggestionContainer);
        // android:divider="@null" in the XML file crashes on Android N and O
        // when running as a WebView (b/38346876).
        mSuggestionListView.setDivider(null);

        mListFooter =
                (LinearLayout) inflater.inflate(R.layout.text_edit_suggestion_list_footer, null);
        mSuggestionListView.addFooterView(mListFooter, null, false);

        mSuggestionListView.setAdapter(new SuggestionAdapter());
        mSuggestionListView.setOnItemClickListener(this);

        mDivider = mContentView.findViewById(R.id.divider);

        mAddToDictionaryButton = (TextView) mContentView.findViewById(R.id.addToDictionaryButton);
        mAddToDictionaryButton.setOnClickListener(this);

        mDeleteButton = (TextView) mContentView.findViewById(R.id.deleteButton);
        mDeleteButton.setOnClickListener(this);
    }

    /**
     * Dismisses the text suggestion menu (called by TextSuggestionHost when certain events occur,
     * for example device rotation).
     */
    public void dismiss() {
        mPopupWindow.dismiss();
    }

    /**
     * Used by TextSuggestionHost to determine if the text suggestion menu is currently visible.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /**
     * Used by TextSuggestionHost to update {@link WindowAndroid} to the current one.
     */
    public void updateWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    private void addToDictionary() {
        final Intent intent = new Intent(ACTION_USER_DICTIONARY_INSERT);

        String wordToAdd = mHighlightedText;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            // There was a bug in Jelly Bean, fixed in the initial version of KitKat, that can cause
            // a crash if the word we try to add is too long. The "add to dictionary" intent uses an
            // EditText widget to show the word about to be added (and allow the user to edit it).
            // It has a maximum length of 48 characters. If a word is longer than this, it will be
            // truncated, but the intent will try to select the full length of the word, causing a
            // crash.

            // KitKit and later still truncate the word, but avoid the crash.
            if (wordToAdd.length() > ADD_TO_DICTIONARY_MAX_LENGTH_ON_JELLY_BEAN) {
                wordToAdd = wordToAdd.substring(0, ADD_TO_DICTIONARY_MAX_LENGTH_ON_JELLY_BEAN);
            }
        }

        intent.putExtra(USER_DICTIONARY_EXTRA_WORD, wordToAdd);
        intent.setFlags(intent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private class SuggestionAdapter extends BaseAdapter {
        private LayoutInflater mInflater =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        @Override
        public int getCount() {
            return mNumberOfSuggestionsToUse;
        }

        @Override
        public Object getItem(int position) {
            return getSuggestionItem(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TextView textView = (TextView) convertView;
            if (textView == null) {
                textView = (TextView) mInflater.inflate(
                        R.layout.text_edit_suggestion_item, parent, false);
            }

            textView.setText(getSuggestionText(position));
            return textView;
        }
    }

    private void measureContent() {
        // Make the menu wide enough to fit its widest item.
        int width = UiUtils.computeMaxWidthOfListAdapterItems(mSuggestionListView.getAdapter());
        width += mContentView.getPaddingLeft() + mContentView.getPaddingRight();

        final int verticalMeasure = View.MeasureSpec.makeMeasureSpec(
                mDisplayMetrics.heightPixels, View.MeasureSpec.AT_MOST);
        mContentView.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY), verticalMeasure);
        mPopupWindow.setWidth(width);
    }

    private void updateDividerVisibility() {
        // If we don't have any spell check suggestions, "Add to dictionary" will be the first menu
        // item, and we shouldn't show a divider above it.
        if (mNumberOfSuggestionsToUse == 0) {
            mDivider.setVisibility(View.GONE);
        } else {
            mDivider.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Called by TextSuggestionHost to tell this class what text is currently highlighted (so it can
     * be added to the dictionary if requested).
     */
    protected void show(double caretXPx, double caretYPx, String highlightedText) {
        mNumberOfSuggestionsToUse = getSuggestionsCount();
        mHighlightedText = highlightedText;

        mActivity = mWindowAndroid.getActivity().get();
        // Note: the Activity can be null here if we're in a WebView that was created without
        // using an Activity. So all code in this class should handle this case.
        if (mActivity != null) {
            mDisplayMetrics = mActivity.getResources().getDisplayMetrics();
        } else {
            // Getting the DisplayMetrics from the passed-in context doesn't handle multi-window
            // mode as well, but it's good enough for the "improperly-created WebView" case
            mDisplayMetrics = mContext.getResources().getDisplayMetrics();
        }

        // In single-window mode, we need to get the status bar height to make sure we don't try to
        // draw on top of it (we can't draw on top in older versions of Android).
        // In multi-window mode, as of Android N, the behavior is as follows:
        //
        // Portrait mode, top window: the window height does not include the height of the status
        // bar, but drawing at Y position 0 starts at the top of the status bar.
        //
        // Portrait mode, bottom window: the window height does not include the height of the status
        // bar, and the status bar isn't touching the window, so we can't draw on it regardless.
        //
        // Landscape mode: the window height includes the whole height of the keyboard
        // (Google-internal b/63405914), so we are unable to handle this case properly.
        //
        // For our purposes, we don't worry about if we're drawing over the status bar in
        // multi-window mode, but we need to make sure we don't do it in single-window mode (in case
        // we're on an old version of Android).
        int statusBarHeight = 0;
        if (mActivity != null && !ApiCompatibilityUtils.isInMultiWindowMode(mActivity)) {
            Rect rect = new Rect();
            mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(rect);
            statusBarHeight = rect.top;
        }

        // We determine the maximum number of suggestions we can show by taking the available
        // height in the window, subtracting the height of the list footer (divider, add to
        // dictionary button, delete button), and dividing by the height of a suggestion item.
        mListFooter.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        final int verticalSpaceAvailableForSuggestions = mDisplayMetrics.heightPixels
                - statusBarHeight - mListFooter.getMeasuredHeight() - 2 * mPopupVerticalMargin
                - mContentView.getPaddingTop() - mContentView.getPaddingBottom();
        final int itemHeight = mContext.getResources().getDimensionPixelSize(
                R.dimen.text_edit_suggestion_item_layout_height);
        final int maxItemsToShow = verticalSpaceAvailableForSuggestions > 0
                ? verticalSpaceAvailableForSuggestions / itemHeight
                : 0;

        mNumberOfSuggestionsToUse = Math.min(mNumberOfSuggestionsToUse, maxItemsToShow);
        // If we're not showing any suggestions, hide the divider before "Add to dictionary" and
        // "Delete".
        updateDividerVisibility();
        measureContent();

        final int width = mContentView.getMeasuredWidth();
        final int height = mContentView.getMeasuredHeight();

        // Horizontally center the menu on the caret location, and vertically position the menu
        // under the caret.
        int positionX = (int) Math.round(caretXPx - width / 2.0f);
        int positionY = (int) Math.round(caretYPx);

        // We get the insertion point coords relative to the viewport.
        // We need to render the popup relative to the window.
        final int[] positionInWindow = new int[2];
        mParentView.getLocationInWindow(positionInWindow);

        positionX += positionInWindow[0];
        positionY += positionInWindow[1];

        // Subtract off the container's top padding to get the proper alignment with the caret.
        // Note: there is no explicit padding set. On Android L and later, we use elevation to draw
        // a drop shadow and there is no top padding. On pre-L, we instead use a background image,
        // which results in some implicit padding getting added that we need to account for.
        positionY -= mContentView.getPaddingTop();

        // Horizontal clipping: if part of the menu (except the shadow) would fall off the left
        // or right edge of the screen, shift the menu to keep it on-screen.
        final int menuAtRightEdgeOfWindowPositionX =
                mDisplayMetrics.widthPixels - width + mContentView.getPaddingRight();
        positionX = Math.min(menuAtRightEdgeOfWindowPositionX, positionX);
        positionX = Math.max(-mContentView.getPaddingLeft(), positionX);

        // Vertical clipping: if part of the menu or its bottom margin would fall off the bottom of
        // the screen, shift it up to keep it on-screen.
        positionY = Math.min(positionY,
                mDisplayMetrics.heightPixels - height - mContentView.getPaddingTop()
                        - mPopupVerticalMargin);

        mPopupWindow.showAtLocation(mParentView, Gravity.NO_GRAVITY, positionX, positionY);
    }

    @Override
    public void onClick(View v) {
        if (v == mAddToDictionaryButton) {
            addToDictionary();
            mTextSuggestionHost.onNewWordAddedToDictionary(mHighlightedText);
            mDismissedByItemTap = true;
            mPopupWindow.dismiss();
        } else if (v == mDeleteButton) {
            mTextSuggestionHost.deleteActiveSuggestionRange();
            mDismissedByItemTap = true;
            mPopupWindow.dismiss();
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        // Ignore taps somewhere in the list footer (divider, "Add to dictionary", "Delete") that
        // don't get handled by a button.
        if (position >= mNumberOfSuggestionsToUse) {
            return;
        }

        applySuggestion(position);
        mDismissedByItemTap = true;
        mPopupWindow.dismiss();
    }

    @Override
    public void onDismiss() {
        mTextSuggestionHost.onSuggestionMenuClosed(mDismissedByItemTap);
        mDismissedByItemTap = false;
    }

    /**
     * @return The popup's content view.
     */
    @VisibleForTesting
    public View getContentViewForTesting() {
        return mContentView;
    }
}
