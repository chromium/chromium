// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.PopupWindow;

/**
 * Paste popup implementation based on TextView.PastePopupMenu.
 */
public class LegacyPastePopupMenu implements OnClickListener, PastePopupMenu {
    private final View mParent;
    private final PastePopupMenuDelegate mDelegate;
    private final Context mContext;
    private final PopupWindow mContainer;
    private int mRawPositionX;
    private int mRawPositionY;
    private int mStatusBarHeight;
    private View mPasteView;
    private final int mPasteViewLayout;
    private final int mLineOffsetY;
    private final int mWidthOffsetX;

    // status_bar_height is not a public framework resource, so we have to getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    public LegacyPastePopupMenu(
            Context context, View parent, final PastePopupMenuDelegate delegate) {
        mParent = parent;
        mDelegate = delegate;
        mContext = context;
        mContainer = new PopupWindow(mContext, null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mContainer.setAnimationStyle(0);

        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        final int[] popupLayoutAttrs = {
                android.R.attr.textEditPasteWindowLayout,
        };

        TypedArray attrs = mContext.getTheme().obtainStyledAttributes(popupLayoutAttrs);
        mPasteViewLayout = attrs.getResourceId(attrs.getIndex(0), 0);

        attrs.recycle();

        // Convert line offset dips to pixels.
        mLineOffsetY = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 5.0f, mContext.getResources().getDisplayMetrics());
        mWidthOffsetX = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 30.0f, mContext.getResources().getDisplayMetrics());

        final int statusBarHeightResourceId =
                mContext.getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (statusBarHeightResourceId > 0) {
            mStatusBarHeight =
                    mContext.getResources().getDimensionPixelSize(statusBarHeightResourceId);
        }
    }

    @Override
    public void show(Rect selectionRect) {
        hide();
        updateContent();
        positionAt(selectionRect.left, selectionRect.bottom);
    }

    @Override
    public void hide() {
        mContainer.dismiss();
    }

    @Override
    public void onClick(View v) {
        paste();
        hide();
    }

    private void positionAt(int x, int y) {
        if (mRawPositionX == x && mRawPositionY == y) return;
        mRawPositionX = x;
        mRawPositionY = y;

        final View contentView = mContainer.getContentView();
        final int width = contentView.getMeasuredWidth();
        final int height = contentView.getMeasuredHeight();

        int positionX = (int) (x - width / 2.0f);
        int positionY = y - height - mLineOffsetY;

        int minOffsetY = 0;
        if (mParent.getSystemUiVisibility() == View.SYSTEM_UI_FLAG_VISIBLE) {
            minOffsetY = mStatusBarHeight;
        }

        final int screenWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        if (positionY < minOffsetY) {
            // Vertical clipping, move under edited line and to the side of insertion cursor
            // TODO bottom clipping in case there is no system bar
            positionY += height;
            positionY += mLineOffsetY;

            // Move to right hand side of insertion cursor by default. TODO RTL text.
            final int handleHalfWidth = mWidthOffsetX / 2;

            if (x + width < screenWidth) positionX += handleHalfWidth + width / 2;
            else positionX -= handleHalfWidth + width / 2;
        } else {
            // Horizontal clipping
            positionX = Math.max(0, positionX);
            positionX = Math.min(screenWidth - width, positionX);
        }

        // Offseting with location in window.
        final int[] coords = new int[2];
        mParent.getLocationInWindow(coords);
        positionX += coords[0];
        positionY += coords[1];

        mContainer.showAtLocation(mParent, Gravity.NO_GRAVITY, positionX, positionY);
    }

    private void updateContent() {
        if (mPasteView == null) {
            final LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            if (inflater != null) mPasteView = inflater.inflate(mPasteViewLayout, null);

            if (mPasteView == null) {
                throw new IllegalArgumentException("Unable to inflate TextEdit paste window");
            }

            final int size = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
            mPasteView.setLayoutParams(new LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            mPasteView.measure(size, size);

            mPasteView.setOnClickListener(this);
        }
        mContainer.setContentView(mPasteView);
    }

    private void paste() {
        mDelegate.paste();
    }
}
