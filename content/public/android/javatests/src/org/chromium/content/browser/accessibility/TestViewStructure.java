// Copyright 2021 The Chromium Authors. All rights reserved.
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
public class TestViewStructure extends ViewStructure implements TestViewStructureInterface {
    private CharSequence mText;
    private String mClassName;
    private Bundle mBundle;
    private int mChildCount;
    private ArrayList<TestViewStructure> mChildren = new ArrayList<TestViewStructure>();
    private boolean mDone = true;
    private boolean mDumpHtmlTags;

    public TestViewStructure() {}

    @Override
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

    @Override
    public String getClassName() {
        return mClassName;
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
        return 0;
    }

    @Override
    public int getTextSelectionEnd() {
        return 0;
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
    public void setChildCount(int num) {}

    @Override
    public int addChildCount(int num) {
        int index = mChildCount;
        mChildCount += num;
        mChildren.ensureCapacity(mChildCount);
        return index;
    }

    @Override
    public int getChildCount() {
        return mChildren.size();
    }

    @Override
    public void dumpHtmlTags() {
        mDumpHtmlTags = true;
    }

    @Override
    public ViewStructure newChild(int index) {
        TestViewStructure viewStructure = new TestViewStructure();
        if (index < mChildren.size()) {
            mChildren.set(index, viewStructure);
        } else if (index == mChildren.size()) {
            mChildren.add(viewStructure);
        } else {
            // Failed intentionally, we shouldn't run into this case.
            mChildren.add(index, viewStructure);
        }
        return viewStructure;
    }

    @Override
    public TestViewStructureInterface getChild(int index) {
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
    public void setText(CharSequence text, int selectionStart, int selectionEnd) {}

    @Override
    public void setTextStyle(float size, int fgColor, int bgColor, int style) {}

    @Override
    public void setTextLines(int[] charOffsets, int[] baselines) {}

    @Override
    public void setWebDomain(String webDomain) {}
}
