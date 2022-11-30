// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Menu;
import android.view.MenuItem;

import androidx.core.content.ContextCompat;

import org.chromium.base.Log;

/** Utility methods for chromoting code. */
public abstract class ChromotingUtil {
    private static final String TAG = "Chromoting";

    /**
     * Tints all icons of a toolbar menu so they have the same color as the 'back' navigation icon
     * and the three-dots overflow icon.
     * @param context Context for getting theme and resource information.
     * @param menu Menu with icons to be tinted.
     */
    public static void tintMenuIcons(Context context, Menu menu) {
        int color = getColorAttribute(context, R.attr.colorControlNormal);
        int items = menu.size();
        for (int i = 0; i < items; i++) {
            ChromotingUtil.tintMenuIcon(menu.getItem(i), color);
        }
    }

    /**
     * Sets a color filter on the specified MenuItem.
     * @param menuItem MenuItem to tint.
     * @param color Color to set on the menuItem.
     */
    public static void tintMenuIcon(MenuItem menuItem, int color) {
        Drawable icon = menuItem.getIcon();
        if (icon != null) {
            icon.mutate().setColorFilter(color, PorterDuff.Mode.SRC_IN);
        }
    }

    /**
     * Returns a color from a theme attribute.
     * @param context Context with resources to look up.
     * @param attribute Attribute such as R.attr.colorControlNormal.
     * @return Color value.
     * @throws Resources.NotFoundException
     */
    public static int getColorAttribute(Context context, int attribute) {
        TypedValue typedValue = new TypedValue();
        if (!context.getTheme().resolveAttribute(attribute, typedValue, true)) {
            throw new Resources.NotFoundException("Attribute not found.");
        }

        if (typedValue.resourceId != 0) {
            // Attribute is a resource.
            return ContextCompat.getColor(context, typedValue.resourceId);
        } else if (typedValue.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && typedValue.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            // Attribute is a raw color value.
            return typedValue.data;
        } else {
            throw new Resources.NotFoundException("Attribute not a color.");
        }
    }

    /**
     * Starts a new Activity only if the system can resolve the given Intent. Useful for implicit
     * intents where the system might not have an application that can handle it.
     * @param context The parent context.
     * @param intent The (implicit) intent to launch.
     * @return True if the intent was resolved.
     */
    @SuppressWarnings("QueryPermissionsNeeded")
    public static boolean startActivitySafely(Context context, Intent intent) {
        if (intent.resolveActivity(context.getPackageManager()) == null) {
            Log.w(TAG, "Unable to resolve activity for: %s", intent);
            return false;
        }
        context.startActivity(intent);
        return true;
    }

    /** Launches an external web browser or application. */
    public static boolean openUrl(Activity parentActivity, Uri uri) {
        return startActivitySafely(parentActivity, new Intent(Intent.ACTION_VIEW, uri));
    }

    /**
     * Converts a measurement in px to dp (density independent pixel).
     *
     * @param metrics The metrics used for conversion.
     * @param value The value in px to be converted.
     * @return The converted result in dp.
     */
    public static int pxToDp(DisplayMetrics metrics, int value) {
        // +0.5f to round up the result.
        return (int) (value / metrics.density + 0.5f);
    }
}
