// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_HEIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_LEFT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_WIDTH;

import static java.lang.String.CASE_INSENSITIVE_ORDER;

import android.graphics.Matrix;
import android.os.Bundle;
import android.os.LocaleList;
import android.text.TextUtils;
import android.util.Pair;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Implementation of ViewStructure that allows us to print it out as a
 * string and assert that the data in the structure is correct.
 */
public class TestViewStructure extends ViewStructure {
    private CharSequence mText;
    private String mClassName;
    private Bundle mBundle;
    private HtmlInfo mHtmlInfo;
    private int mChildCount;
    private ArrayList<TestViewStructure> mChildren = new ArrayList<TestViewStructure>();
    private boolean mDone = true;
    private float mTextSize;
    private int mFgColor;
    private int mBgColor;
    private int mStyle;
    private int mSelectionStart;
    private int mSelectionEnd;

    public TestViewStructure() {}

    public boolean isDone() {
        if (!mDone) return false;

        for (TestViewStructure child : mChildren) {
            if (!child.isDone()) return false;
        }

        return true;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        recursiveDumpToString(builder, 0);
        return builder.toString().trim();
    }

    public String getClassName() {
        return mClassName;
    }

    public float getTextSize() {
        return mTextSize;
    }

    public int getFgColor() {
        return mFgColor;
    }

    public int getBgColor() {
        return mBgColor;
    }

    public int getStyle() {
        return mStyle;
    }

    private static String bundleToString(Bundle extras) {
        // Sort keys to ensure consistent output of tests.
        List<String> sortedKeySet = new ArrayList<String>(extras.keySet());
        Collections.sort(sortedKeySet, CASE_INSENSITIVE_ORDER);

        List<String> bundleStrings = new ArrayList<>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (String key : sortedKeySet) {
            // Bundle extras related to bounding boxes should be ignored so the tests can safely
            // run on varying devices and not be screen-dependent.
            if (key.equals(EXTRAS_KEY_UNCLIPPED_LEFT)
                    || key.equals(EXTRAS_KEY_UNCLIPPED_TOP)
                    || key.equals(EXTRAS_KEY_UNCLIPPED_WIDTH)
                    || key.equals(EXTRAS_KEY_UNCLIPPED_HEIGHT)
                    || key.equals("url")) {
                continue;
            }

            // Simplify the key String before printing to make test outputs easier to read.
            bundleStrings.add(
                    key.replace("AccessibilityNodeInfo.", "")
                            + "=\""
                            + extras.get(key).toString()
                            + "\"");
        }
        builder.append(TextUtils.join(", ", bundleStrings)).append("]");

        return builder.toString();
    }

    private void recursiveDumpToString(StringBuilder builder, int indent) {
        // We do not want to print the root node, start at the WebView.
        if (mClassName == null) {
            assert indent == 0;
            assert mChildCount == 1;
            mChildren.get(0).recursiveDumpToString(builder, indent);
            return;
        }

        for (int i = 0; i < indent; i++) {
            builder.append("++");
        }

        // Print classname first, but only print content after the last period to remove redundancy.
        assert mClassName != null : "Classname should never be null";
        assert !mClassName.contains("\\.") : "Classname should contain periods";
        String[] classNameParts = mClassName.split("\\.");
        builder.append(classNameParts[classNameParts.length - 1]);

        // Print text unless it is empty (null is allowed).
        if (mText == null) {
            builder.append(" text:\"null\"");
        } else if (!mText.toString().isEmpty()) {
            builder.append(" text:\"").append(mText.toString().replace("\n", "\\n")).append("\"");
        }

        // Print text selection values if present.
        if (mSelectionStart != 0 || getTextSelectionEnd() != 0) {
            builder.append(" textSelectionStart:").append(mSelectionStart);
            builder.append(" textSelectionEnd:").append(mSelectionEnd);
        }

        // Print font styling values.
        builder.append(" textSize:").append(String.format("%.2f", mTextSize));
        builder.append(" style:").append(mStyle);
        if (mFgColor != 0xFF000000) {
            builder.append(" fgColor:").append(mFgColor);
        }
        if (mBgColor != 0 && mBgColor != -1) {
            builder.append(" bgColor:").append(mBgColor);
        }

        // Print Bundle extras and htmlInfo attributes.
        if (mBundle != null) {
            builder.append(" bundle:").append(bundleToString(mBundle));
        }
        if (mHtmlInfo != null) {
            builder.append(" htmlInfo:[");
            for (Pair<String, String> pair : mHtmlInfo.getAttributes()) {
                builder.append(" {").append(pair.first).append(",").append(pair.second).append("}");
            }
            builder.append(" ]");
        }

        builder.append("\n");

        for (TestViewStructure child : mChildren) {
            child.recursiveDumpToString(builder, indent + 1);
        }
    }

    @Override
    public void setAlpha(float alpha) {}

    @Override
    public void setAccessibilityFocused(boolean state) {}

    @Override
    public void setCheckable(boolean state) {}

    @Override
    public void setChecked(boolean state) {}

    @Override
    public void setActivated(boolean state) {}

    @Override
    public CharSequence getText() {
        return mText;
    }

    @Override
    public int getTextSelectionStart() {
        return mSelectionStart;
    }

    @Override
    public int getTextSelectionEnd() {
        return mSelectionEnd;
    }

    @Override
    public CharSequence getHint() {
        return null;
    }

    @Override
    public Bundle getExtras() {
        if (mBundle == null) mBundle = new Bundle();
        return mBundle;
    }

    @Override
    public boolean hasExtras() {
        return mBundle != null;
    }

    @Override
    public void setChildCount(int num) {
        mChildren.ensureCapacity(num);
        for (int i = mChildCount; i < num; i++) {
            mChildCount++;
            mChildren.add(null);
        }
    }

    @Override
    public int addChildCount(int num) {
        int index = mChildCount;
        mChildren.ensureCapacity(mChildCount + num);
        for (int i = 0; i < num; i++) {
            mChildCount++;
            mChildren.add(null);
        }
        return index;
    }

    @Override
    public int getChildCount() {
        return mChildren.size();
    }

    @Override
    public ViewStructure newChild(int index) {
        TestViewStructure viewStructure = new TestViewStructure();
        // Note: this will fail if index is out of bounds, to match
        // the behavior of AssistViewStructure in practice.
        mChildren.set(index, viewStructure);
        return viewStructure;
    }

    public TestViewStructure getChild(int index) {
        return mChildren.get(index);
    }

    @Override
    public ViewStructure asyncNewChild(int index) {
        TestViewStructure result = (TestViewStructure) newChild(index);
        result.mDone = false;
        return result;
    }

    @Override
    public void asyncCommit() {
        assert !mDone;
        mDone = true;
    }

    @Override
    public AutofillId getAutofillId() {
        return null;
    }

    @Override
    public HtmlInfo.Builder newHtmlInfoBuilder(String tag) {
        return null;
    }

    @Override
    public void setAutofillHints(String[] arg0) {}

    @Override
    public void setAutofillId(AutofillId arg0) {}

    @Override
    public void setAutofillId(AutofillId arg0, int arg1) {}

    @Override
    public void setAutofillOptions(CharSequence[] arg0) {}

    @Override
    public void setAutofillType(int arg0) {}

    @Override
    public void setAutofillValue(AutofillValue arg0) {}

    @Override
    public void setId(int id, String packageName, String typeName, String entryName) {}

    @Override
    public void setDimens(int left, int top, int scrollX, int scrollY, int width, int height) {}

    @Override
    public void setElevation(float elevation) {}

    @Override
    public void setEnabled(boolean state) {}

    @Override
    public void setClickable(boolean state) {}

    @Override
    public void setLongClickable(boolean state) {}

    @Override
    public void setContextClickable(boolean state) {}

    @Override
    public void setFocusable(boolean state) {}

    @Override
    public void setFocused(boolean state) {}

    @Override
    public void setClassName(String className) {
        mClassName = className;
    }

    @Override
    public void setContentDescription(CharSequence contentDescription) {}

    @Override
    public void setDataIsSensitive(boolean arg0) {}

    @Override
    public void setHint(CharSequence hint) {}

    @Override
    public void setHtmlInfo(HtmlInfo htmlInfo) {
        mHtmlInfo = htmlInfo;
    }

    @Override
    public void setInputType(int arg0) {}

    @Override
    public void setLocaleList(LocaleList arg0) {}

    @Override
    public void setOpaque(boolean arg0) {}

    @Override
    public void setTransformation(Matrix matrix) {}

    @Override
    public void setVisibility(int visibility) {}

    @Override
    public void setSelected(boolean state) {}

    @Override
    public void setText(CharSequence text) {
        mText = text;
    }

    @Override
    public void setText(CharSequence text, int selectionStart, int selectionEnd) {
        mText = text;
        mSelectionStart = selectionStart;
        mSelectionEnd = selectionEnd;
    }

    @Override
    public void setTextStyle(float size, int fgColor, int bgColor, int style) {
        mTextSize = size;
        mFgColor = fgColor;
        mBgColor = bgColor;
        mStyle = style;
    }

    @Override
    public void setTextLines(int[] charOffsets, int[] baselines) {}

    @Override
    public void setWebDomain(String webDomain) {}
}
