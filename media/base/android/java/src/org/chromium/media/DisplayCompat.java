// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static android.content.Context.UI_MODE_SERVICE;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.UiModeManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Point;
import android.os.Build;
import android.text.TextUtils;
import android.view.Display;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import java.lang.reflect.Method;
import java.util.ArrayList;

/**
 * A class for retrieving the physical display size from a device. This is necessary because
 * Display.Mode.getPhysicalDisplaySize might not report the real physical display size
 * because most ATV devices don't report all available modes correctly. In this case there is no
 * way to find out whether a device is capable to display 4k content. This class offers a
 * workaround for this problem.
 * Note: This code is copied from androidx.core.view.DisplayCompat.
 */
public final class DisplayCompat {
    private static final int DISPLAY_SIZE_4K_WIDTH = 3840;
    private static final int DISPLAY_SIZE_4K_HEIGHT = 2160;

    private DisplayCompat() {
        // This class is non-instantiable.
    }

    /**
     * Gets the supported modes of the given display where at least one of the modes is flagged
     * as isNative(). Note that a native mode might not wrap any Display.Mode object in case
     * the display returns no mode with the physical display size.
     *
     * @return an array of supported modes where at least one of the modes is native which
     * contains the physical display size
     */
    @NonNull
    @SuppressLint("ArrayReturn")
    public static ModeCompat[] getSupportedModes(
            @NonNull Context context, @NonNull Display display) {
        Point physicalDisplaySize = getPhysicalDisplaySize(context, display);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Display.Mode class and display.getSupportedModes() exist
            Display.Mode[] supportedModes = display.getSupportedModes();
            ArrayList<ModeCompat> supportedModesCompat = new ArrayList<>(supportedModes.length);
            boolean nativeModeExists = false;
            for (int i = 0; i < supportedModes.length; i++) {
                if (physicalSizeEquals(supportedModes[i], physicalDisplaySize)) {
                    // Current mode has native resolution, flag it accordingly
                    supportedModesCompat.add(i, new ModeCompat(supportedModes[i], true));
                    nativeModeExists = true;
                } else {
                    supportedModesCompat.add(i, new ModeCompat(supportedModes[i], false));
                }
            }
            if (!nativeModeExists) {
                // If no mode with physicalDisplaySize dimension exists, add the mode with the
                // native display resolution
                supportedModesCompat.add(new ModeCompat(physicalDisplaySize));
            }
            return supportedModesCompat.toArray(new ModeCompat[0]);
        } else {
            // previous to Android M Display.Mode and Display.getSupportedModes() did not exist,
            // hence the only supported mode is the native display resolution
            return new ModeCompat[] {new ModeCompat(physicalDisplaySize)};
        }
    }

    /**
     * Returns whether the app is running on a TV device
     *
     * @return true iff the app is running on a TV device
     */
    public static boolean isTv(@NonNull Context context) {
        // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(UI_MODE_SERVICE);
        return uiModeManager != null
                && uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;
    }

    /**
     * Parses a string which represents the display-size which contains 'x' as a delimiter
     * between two integers representing the display's width and height and returns the
     * display size as a Point object.
     *
     * @param displaySize a string
     * @return a Point object containing the size in x and y direction in pixels
     * @throws NumberFormatException in case the integers cannot be parsed
     */
    private static Point parseDisplaySize(@NonNull String displaySize)
            throws NumberFormatException {
        String[] displaySizeParts = displaySize.trim().split("x", -1);
        if (displaySizeParts.length == 2) {
            int width = Integer.parseInt(displaySizeParts[0]);
            int height = Integer.parseInt(displaySizeParts[1]);
            if (width > 0 && height > 0) {
                return new Point(width, height);
            }
        }
        throw new NumberFormatException();
    }

    /**
     * Reads a system property and returns its string value.
     *
     * @param name the name of the system property
     * @return the result string or null if an exception occurred
     */
    @Nullable
    private static String getSystemProperty(String name) {
        try {
            @SuppressLint("PrivateApi")
            Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            Method getMethod = systemProperties.getMethod("get", String.class);
            return (String) getMethod.invoke(systemProperties, name);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Returns true if mode.getPhysicalWidth and mode.getPhysicalHeight are equal to the given size
     *
     * @param mode a Display.Mode object
     * @param size a Point object representing the size in horizontal and vertical direction
     */
    @RequiresApi(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
    private static boolean physicalSizeEquals(Display.Mode mode, Point size) {
        return (mode.getPhysicalWidth() == size.x && mode.getPhysicalHeight() == size.y)
                || (mode.getPhysicalWidth() == size.y && mode.getPhysicalHeight() == size.x);
    }

    /**
     * Helper function to determine the physical display size from the system properties only. On
     * Android TVs it is common for the UI to be configured for a lower resolution than SurfaceViews
     * can output. Before API 26 the Display object does not provide a way to identify this case,
     * and up to and including API 28 many devices still do not correctly set their hardware
     * composer output size.
     *
     * @return the physical display size, in pixels or null if the information is not available
     */
    @Nullable
    private static Point parsePhysicalDisplaySizeFromSystemProperties(
            @NonNull String property, @NonNull Display display) {
        if (display.getDisplayId() == Display.DEFAULT_DISPLAY) {
            // Check the system property for display size. From API 28 treble may prevent the
            // system from writing sys.display-size so we check vendor.display-size instead.
            String displaySize = getSystemProperty(property);
            // If we managed to read the display size, attempt to parse it.
            if (!TextUtils.isEmpty(displaySize)) {
                try {
                    return parseDisplaySize(displaySize);
                } catch (NumberFormatException e) {
                    // Do nothing for now, null is returned in the end
                }
            }
        }
        // Unable to determine display size from system properties
        return null;
    }

    /**
     * Gets the physical size of the given display in pixels. The size is collected in the
     * following order:
     * 1) sys.display-size if API < 28 (P) and the system-property is set
     * 2) vendor.display-size if API >= 28 (P) and the system-property is set
     * 3) physical width and height from display.getMode() for API >= 23
     * 4) display.getRealSize() for API >= 17
     * 5) display.getSize()
     *
     * @return the physical display size, in pixels
     */
    private static Point getPhysicalDisplaySize(
            @NonNull Context context, @NonNull Display display) {
        Point displaySize = Build.VERSION.SDK_INT < Build.VERSION_CODES.P
                ? parsePhysicalDisplaySizeFromSystemProperties("sys.display-size", display)
                : parsePhysicalDisplaySizeFromSystemProperties("vendor.display-size", display);
        if (displaySize != null) {
            return displaySize;
        } else if (isSonyBravia4kTv(context)) {
            // Sony Android TVs advertise support for 4k output via a system feature.
            return new Point(DISPLAY_SIZE_4K_WIDTH, DISPLAY_SIZE_4K_HEIGHT);
        } else {
            // Unable to retrieve the physical display size from system properties, get display
            // size from the framework API. Note that this might not be the actual physical
            // display size but the, possibly down-scaled, UI size.
            displaySize = new Point();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                Display.Mode mode = display.getMode();
                displaySize.x = mode.getPhysicalWidth();
                displaySize.y = mode.getPhysicalHeight();
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
                display.getRealSize(displaySize);
            } else {
                display.getSize(displaySize);
            }
        }
        return displaySize;
    }

    /**
     * Determines whether the connected display is a 4k capable Sony TV.
     *
     * @return true if the display is a Sony BRAVIA TV that supports 4k
     */
    private static boolean isSonyBravia4kTv(@NonNull Context context) {
        return isTv(context) && "Sony".equals(Build.MANUFACTURER)
                && Build.MODEL.startsWith("BRAVIA")
                && context.getPackageManager().hasSystemFeature("com.sony.dtv.hardware.panel.qfhd");
    }

    /**
     * Compat class which provides an additional isNative() field. This field indicates whether a
     * mode is native, which is important when searching for the highest possible native
     * resolution of a display.
     */
    public static final class ModeCompat {
        private final Display.Mode mMode;
        private final Point mPhysicalDisplaySize;
        private final boolean mIsNative;

        /**
         * Package private constructor which creates a native ModeCompat object that does not
         * wrap any Display.Mode object but only contains the given display size
         *
         * @param physicalDisplaySize a Point object representing the display size in pixels
         *                            (Point.x horizontal and Point.y vertical size)
         */
        ModeCompat(@NonNull Point physicalDisplaySize) {
            if (physicalDisplaySize == null) {
                throw new NullPointerException("physicalDisplaySize == null");
            }

            mIsNative = true;
            mPhysicalDisplaySize = physicalDisplaySize;
            mMode = null;
        }

        /**
         * Package private constructor which creates a non-native ModeCompat and wraps the given
         * Mode object
         *
         * @param mode a Display.Mode object
         */
        @RequiresApi(Build.VERSION_CODES.M)
        @TargetApi(Build.VERSION_CODES.M)
        ModeCompat(@NonNull Display.Mode mode, boolean isNative) {
            if (mode == null) {
                throw new NullPointerException("Display.Mode == null, can't wrap a null reference");
            }

            mIsNative = isNative;
            // This simplifies the getPhysicalWidth() / getPhysicalHeight functions below
            mPhysicalDisplaySize = new Point(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            mMode = mode;
        }

        /**
         * Returns the physical width of the given display when configured in this mode
         *
         * @return the physical screen width in pixels
         */
        public int getPhysicalWidth() {
            return mPhysicalDisplaySize.x;
        }

        /**
         * Returns the physical height of the given display when configured in this mode
         *
         * @return the physical screen height in pixels
         */
        public int getPhysicalHeight() {
            return mPhysicalDisplaySize.y;
        }

        /**
         * Function to get the wrapped object
         *
         * @return the wrapped Display.Mode object or null if there was no matching mode for the
         * native resolution.
         */
        @RequiresApi(Build.VERSION_CODES.M)
        @Nullable
        public Display.Mode toMode() {
            return mMode;
        }

        /**
         * This field indicates whether a mode is native, which is important when searching for
         * the highest possible native resolution of a display.
         *
         * @return true if this is a native mode of the wrapped display
         */
        public boolean isNative() {
            return mIsNative;
        }
    }
}
