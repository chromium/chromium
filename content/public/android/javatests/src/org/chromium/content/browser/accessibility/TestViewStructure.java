// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.graphics.Matrix;
import android.os.Bundle;
import android.os.LocaleList;
import android.text.TextUtils;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;

import java.util.ArrayList;

/**
 * Implementation of ViewStructure that allows us to print it out as a
 * string and assert that the data in the structure is correct.
 */
public class TestViewStructure extends ViewStructure {
    private CharSequence mText;
    private String mClassName;
    private Bundle mBundle;
    private int mChildCount;
    private ArrayList<TestViewStructure> mChildren = new ArrayList<TestViewStructure>();
    private boolean mDone = true;
    private boolean mDumpHtmlTags;
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
        recursiveDumpToString(builder, 0, mDumpHtmlTags);
        return builder.toString();
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

    private void recursiveDumpToString(StringBuilder builder, int indent, boolean dumpHtmlTags) {
        for (int i = 0; i < indent; i++) {
            builder.append("  ");
        }

        if (!TextUtils.isEmpty(mClassName)) {
            builder.append(mClassName);
        }

        if (!TextUtils.isEmpty(mText)) {
            builder.append(" text='");
            builder.append(mText);
            builder.append("'");
        }

        if (mBundle != null) {
            String htmlTag = mBundle.getCharSequence("htmlTag").toString();
            if (dumpHtmlTags && !TextUtils.isEmpty(htmlTag)) {
                builder.append(" htmlTag='");
                builder.append(htmlTag);
                builder.append("'");
            }
        }

        builder.append("\n");

        for (TestViewStructure child : mChildren) {
            child.recursiveDumpToString(builder, indent + 1, dumpHtmlTags);
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

    public void dumpHtmlTags() {
        mDumpHtmlTags = true;
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
    public void setHtmlInfo(HtmlInfo arg0) {}

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
