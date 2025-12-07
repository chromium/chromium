/*
 * Copyright 2018 The Android Open Source Project
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

package org.chromium.ui.recyclerview.widget;

import android.graphics.Canvas;
import android.os.Build;
import android.view.View;

import androidx.core.view.ViewCompat;
import androidx.recyclerview.R;
import androidx.recyclerview.widget.ItemTouchUIUtil;
import androidx.recyclerview.widget.RecyclerView;

import org.jspecify.annotations.NonNull;

/**
 * Package private class to keep implementations. Putting them inside ItemTouchUIUtil makes them
 * public API, which is not desired in this case.
 */
class ItemTouchUIUtilImpl2 implements ItemTouchUIUtil {
    static final ItemTouchUIUtil INSTANCE = new ItemTouchUIUtilImpl2();

    @Override
    public void onDraw(
            @NonNull Canvas c,
            @NonNull RecyclerView recyclerView,
            @NonNull View view,
            float dX,
            float dY,
            int actionState,
            boolean isCurrentlyActive) {
        if (Build.VERSION.SDK_INT >= 21) {
            if (isCurrentlyActive) {
                Object originalElevation = view.getTag(R.id.item_touch_helper_previous_elevation);
                if (originalElevation == null) {
                    originalElevation = ViewCompat.getElevation(view);
                    float newElevation = 1f + findMaxElevation(recyclerView, view);
                    ViewCompat.setElevation(view, newElevation);
                    view.setTag(R.id.item_touch_helper_previous_elevation, originalElevation);
                }
            }
        }

        view.setTranslationX(dX);
        view.setTranslationY(dY);
    }

    private static float findMaxElevation(RecyclerView recyclerView, View itemView) {
        final int childCount = recyclerView.getChildCount();
        float max = 0;
        for (int i = 0; i < childCount; i++) {
            final View child = recyclerView.getChildAt(i);
            if (child == itemView) {
                continue;
            }
            final float elevation = ViewCompat.getElevation(child);
            if (elevation > max) {
                max = elevation;
            }
        }
        return max;
    }

    @Override
    public void onDrawOver(
            @NonNull Canvas c,
            @NonNull RecyclerView recyclerView,
            @NonNull View view,
            float dX,
            float dY,
            int actionState,
            boolean isCurrentlyActive) {}

    @Override
    public void clearView(@NonNull View view) {
        if (Build.VERSION.SDK_INT >= 21) {
            final Object tag = view.getTag(R.id.item_touch_helper_previous_elevation);
            if (tag instanceof Float) {
                ViewCompat.setElevation(view, (Float) tag);
            }
            view.setTag(R.id.item_touch_helper_previous_elevation, null);
        }

        view.setTranslationX(0f);
        view.setTranslationY(0f);
    }

    @Override
    public void onSelected(@NonNull View view) {}
}
