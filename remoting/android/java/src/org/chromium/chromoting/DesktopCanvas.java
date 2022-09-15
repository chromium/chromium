// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;

/**
 * This class is responsible for transforming the desktop image matrix.
 */
public class DesktopCanvas {
    /** Used for floating point comparisons. */
    private static final float EPSILON = 0.0001f;

    /** Maximum allowed zoom level - see {@link #scaleAndRepositionImage()}. */
    private static final float MAX_ZOOM_FACTOR = 100.0f;

    private final RenderStub mRenderStub;
    private final RenderData mRenderData;

    /**
     * The insets from each edge of the screen which avoid display cutouts. Note that this is not
     * a real rectangle as left > right and top > bottom can be true.
     */
    private final Rect mSafeInsets = new Rect();

    /**
     * Represents the desired center of the viewport in image space.  This value may not represent
     * the actual center of the viewport as adjustments are made to ensure as much of the desktop is
     * visible as possible.  This value needs to be a pair of floats so the desktop image can be
     * positioned with sub-pixel accuracy for smoother panning animations at high zoom levels.
     */
    private PointF mDesiredCenter = new PointF();

    /**
     * If System UI exists, this contains the area of the screen which is unobscured by it,
     * otherwise it is empty.
     */
    private Rect mSystemUiScreenRect = new Rect();

    /**
     * Represents the amount of space, in pixels, to shift the image under the viewport.  This value
     * is used to allow panning the image further than would be possible when using the normal
     * boundary values to account for System UI.  This functionality ensures the user can view and
     * interact with any area of the remote image, even when System UI might otherwise obscure it.
     */
    private PointF mViewportOffset = new PointF();

    /**
     * Tracks whether to adjust the size of the viewport to account for System UI. If false, the
     * viewport center is mapped to the center of the screen.  If true, then System UI sizes will be
     * used to determine the center of the viewport.
     */
    private boolean mAdjustViewportSizeForSystemUi;

    /* Used to perform per-frame rendering tasks. */
    private Event.ParameterCallback<Boolean, Void> mFrameRenderedCallback;

    public DesktopCanvas(RenderStub renderStub, RenderData renderData) {
        mRenderStub = renderStub;
        mRenderData = renderData;
    }

    /**
     * Sets the desired center position of the viewport (a.k.a. the cursor position).
     *
     * @param newX The new x coordinate value for the desired center position.
     * @param newY The new y coordinate value for the desired center position.
     */
    public void setCursorPosition(float newX, float newY) {
        updateCursorPosition(newX, newY, getImageBounds());
    }

    /**
     * Sets the center of the viewport using an absolute position (in image coordinates).
     *
     * @param newX The new x coordinate value for the center position of the viewport.
     * @param newY The new y coordinate value for the center position of the viewport.
     */
    public void setViewportCenter(float newX, float newY) {
        updateCursorPosition(newX, newY, getViewportBounds());
    }

    /**
     * Shifts the cursor by the passed in values (in image coordinates).
     *
     * @param deltaX The distance (in image coordinates) to move the cursor along the x-axis.
     * @param deltaY The distance (in image coordinates) to move the cursor along the y-axis.
     * @return A point representing the new cursor position.
     */
    public PointF moveCursorPosition(float deltaX, float deltaY) {
        updateCursorPosition(
                mDesiredCenter.x + deltaX, mDesiredCenter.y + deltaY, getImageBounds());
        return new PointF(mDesiredCenter.x, mDesiredCenter.y);
    }

    /**
     * Shifts the viewport by the passed in values (in image coordinates).
     *
     * @param deltaX The distance (in image coordinates) to move the viewport along the x-axis.
     * @param deltaY The distance (in image coordinates) to move the viewport along the y-axis.
     */
    public void moveViewportCenter(float deltaX, float deltaY) {
        updateCursorPosition(
                mDesiredCenter.x + deltaX, mDesiredCenter.y + deltaY, getViewportBounds());
    }

    /**
     * Handles System UI size and visibility changes.
     *
     * @param parameter The set of values defining the current System UI state.
     */
    public void onSystemUiVisibilityChanged(SystemUiVisibilityChangedEventParameter parameter) {
        mSystemUiScreenRect.set(parameter.left, parameter.top, parameter.right, parameter.bottom);
        stopOffsetReductionAnimation();

        PointF targetOffset;
        if (mSystemUiScreenRect.isEmpty()) {
            targetOffset = new PointF(0.0f, 0.0f);
        } else {
            // If the System UI size has changed such viewport offset is affected, then start an
            // animation to adjust the amount of offset used.  This functionality ensures that we
            // don't leave content-less areas on the screen when the System UI resizes.
            RectF systemUiOverlap = getSystemUiOverlap();
            RectF newBounds = new RectF(-systemUiOverlap.left, -systemUiOverlap.top,
                    systemUiOverlap.right, systemUiOverlap.bottom);
            targetOffset = new PointF(mViewportOffset.x, mViewportOffset.y);
            constrainPointToBounds(targetOffset, newBounds);
        }
        startOffsetReductionAnimation(targetOffset);


        if (mRenderData.initialized()) {
            // The viewport center may have changed so update the position to reflect the new value.
            repositionImage();
        }
    }

    /**
     *  Sets the insets from each edge on the screen that avoid display cutouts.
     *
     * @param insets The insets from each edge on the screen that avoid display cutouts.
     */
    public void setSafeInsets(Rect insets) {
        mSafeInsets.set(insets);

        if (mRenderData.initialized()) {
            // The viewport center may have changed so update the position to reflect the new value.
            repositionImage();
        }
    }

    public void adjustViewportForSystemUi(boolean adjustViewportForSystemUi) {
        mAdjustViewportSizeForSystemUi = adjustViewportForSystemUi;

        if (mRenderData.initialized()) {
            // The viewport center may have changed so update the position to reflect the new value.
            repositionImage();
        }
    }

    /** Resizes the image by zooming it such that the image is displayed without borders. */
    public void resizeImageToFitScreen() {
        // Protect against being called before the image has been initialized.
        if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
            return;
        }

        // Reset to identity so that screen dimensions and image dimensions match up.
        mRenderData.transform.reset();

        float widthRatio = getSafeScreenWidth() / mRenderData.imageWidth;
        float heightRatio = getSafeScreenHeight() / mRenderData.imageHeight;
        float screenToImageScale = Math.max(widthRatio, heightRatio);

        // If the image is smaller than the screen in either dimension, then we want to scale it
        // up to fit both and fill the screen with the image of the remote desktop.
        if (screenToImageScale > 1.0f) {
            mRenderData.transform.setScale(screenToImageScale, screenToImageScale);
        }
    }

    /**
     * Repositions the image by translating and zooming it, to keep the zoom level within sensible
     * limits. The minimum zoom level is chosen to avoid letterboxing on all 4 sides. The
     * maximum zoom level is set arbitrarily, so that the user can zoom out again in a reasonable
     * time, and to prevent arithmetic overflow problems from displaying the image.
     *
     * @param scaleFactor The factor used to zoom the canvas in or out.
     * @param px The center x coordinate for the scale action.
     * @param py The center y coordinate for the scale action.
     * @param centerOnCursor Determines whether the viewport will be translated to the desired
     *                       center position before being adjusted to fit the screen boundaries.
     */
    public void scaleAndRepositionImage(
            float scaleFactor, float px, float py, boolean centerOnCursor) {
        // Avoid division by zero in case this gets called before the image size is initialized.
        if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
            return;
        }

        mRenderData.transform.postScale(scaleFactor, scaleFactor, px, py);

        // Zoom out if the zoom level is too high.
        float currentZoomLevel = mRenderData.transform.mapRadius(1.0f);
        if (currentZoomLevel > MAX_ZOOM_FACTOR) {
            mRenderData.transform.setScale(MAX_ZOOM_FACTOR, MAX_ZOOM_FACTOR);
        }

        // Get image size scaled to screen coordinates.
        float[] imageSize = {mRenderData.imageWidth, mRenderData.imageHeight};
        mRenderData.transform.mapVectors(imageSize);

        if (imageSize[0] < getSafeScreenWidth() && imageSize[1] < getSafeScreenHeight()) {
            // Displayed image is too small in both directions, so apply the minimum zoom
            // level needed to fit either the width or height.
            float scale = Math.min(getSafeScreenWidth() / mRenderData.imageWidth,
                    getSafeScreenHeight() / mRenderData.imageHeight);
            mRenderData.transform.setScale(scale, scale);
        }

        if (centerOnCursor) {
            setCursorPosition(mDesiredCenter.x, mDesiredCenter.y);
        } else {
            // Find the new screen center (it probably changed during the zoom operation) and update
            // the viewport to smoothly track the zoom gesture.
            PointF safeScreenCenter = getSafeScreenCenterPoint();
            float[] mappedPoints = {
                    safeScreenCenter.x - mViewportOffset.x, safeScreenCenter.y - mViewportOffset.y};
            Matrix screenToImage = new Matrix();
            mRenderData.transform.invert(screenToImage);
            screenToImage.mapPoints(mappedPoints);
            // The cursor is mapped to the center of the viewport in this case.
            setViewportCenter(mappedPoints[0], mappedPoints[1]);
        }
    }

    /**
     * Sets the new cursor position, bounded by the given rect, and updates the image transform to
     * reflect the new position.
     */
    private void updateCursorPosition(float newX, float newY, RectF bounds) {
        mDesiredCenter.set(newX, newY);
        constrainPointToBounds(mDesiredCenter, bounds);

        calculateViewportOffset(newX - mDesiredCenter.x, newY - mDesiredCenter.y);

        repositionImage();
    }

    /**
     * Returns the height of the screen (in screen coordinates) for use in calculations involving
     * viewport positioning.
     */
    private float getAdjustedScreenHeight() {
        float adjustedScreenHeight;
        if (mAdjustViewportSizeForSystemUi && !mSystemUiScreenRect.isEmpty()) {
            // Find the center point of the viewport on the screen.
            adjustedScreenHeight = mSystemUiScreenRect.bottom;
        } else {
            adjustedScreenHeight = getSafeScreenHeight();
        }

        return adjustedScreenHeight;
    }

    /**
     * Returns the center position of the viewport (in screen coordinates) taking System UI into
     * account.
     */
    private PointF getViewportScreenCenter() {
        return getAdjustedScreenCenterPoint();
    }

    /**
     * Repositions the image by translating it (without affecting the zoom level).
     */
    private void repositionImage() {
        PointF viewportPosition = new PointF(mDesiredCenter.x, mDesiredCenter.y);
        constrainPointToBounds(viewportPosition, getViewportBounds());
        float[] viewportAdjustment = {viewportPosition.x, viewportPosition.y};
        mRenderData.transform.mapPoints(viewportAdjustment);

        // Adjust the viewport to include the overpan amount.
        viewportAdjustment[0] += mViewportOffset.x;
        viewportAdjustment[1] += mViewportOffset.y;

        // Translate the image to move the viewport to the expected screen location.
        PointF viewportCenter = getViewportScreenCenter();
        mRenderData.transform.postTranslate(
                viewportCenter.x - viewportAdjustment[0], viewportCenter.y - viewportAdjustment[1]);

        mRenderStub.setTransformation(mRenderData.transform);
    }

    /**
     * Updates the given point such that it refers to a coordinate within the bounds provided.
     *
     * @param point The point to adjust, the original object is modified.
     * @param bounds The bounds used to constrain the point.
     */
    private void constrainPointToBounds(PointF point, RectF bounds) {
        if (point.x < bounds.left) {
            point.x = bounds.left;
        } else if (point.x > bounds.right) {
            point.x = bounds.right;
        }

        if (point.y < bounds.top) {
            point.y = bounds.top;
        } else if (point.y > bounds.bottom) {
            point.y = bounds.bottom;
        }
    }

    /** Returns a region which defines the set of valid cursor positions in image space. */
    private RectF getImageBounds() {
        return new RectF(0, 0, mRenderData.imageWidth, mRenderData.imageHeight);
    }

    /** Returns a region which defines the set of valid viewport center values in image space. */
    private RectF getViewportBounds() {
        // The region of allowable viewport values is the imageBound rect, inset by the size of the
        // viewport itself.  This prevents over and under panning of the viewport while still
        // allowing the user to see and interact with all pixels one the desktop image.
        Matrix screenToImage = new Matrix();
        mRenderData.transform.invert(screenToImage);

        float[] screenVectors = {getSafeScreenWidth() / 2, getAdjustedScreenHeight() / 2};
        screenToImage.mapVectors(screenVectors);

        PointF letterboxPadding = getLetterboxPadding();
        float[] letterboxPaddingVectors = {letterboxPadding.x, letterboxPadding.y};
        screenToImage.mapVectors(letterboxPaddingVectors);

        // screenCenter values are 1/2 of a particular screen dimension mapped to image space.
        float screenCenterX = screenVectors[0] - letterboxPaddingVectors[0];
        float screenCenterY = screenVectors[1] - letterboxPaddingVectors[1];
        RectF imageBounds = getImageBounds();
        imageBounds.inset(screenCenterX, screenCenterY);
        return imageBounds;
    }

    /**
     * Returns a region defining the maximum offset distance required to view the entire image
     * given the current amount of System UI overlapping it.
     */
    private RectF getViewportOffsetBounds() {
        // The allowable region is determined by:
        //   - Overlap of the System UI and image content
        //   - Current viewport offset
        //
        // The System UI overlap represents the maximum allowable offset, this is used to bound the
        // viewport movement in each direction.  The current offset is used to prevent 'snapping'
        // the image when the System UI overlap is reduced.
        RectF viewportOffsetRect = new RectF();
        viewportOffsetRect.union(mViewportOffset.x, mViewportOffset.y);

        RectF systemUiOverlap = getSystemUiOverlap();
        return new RectF(Math.min(viewportOffsetRect.left, -systemUiOverlap.left),
                Math.min(viewportOffsetRect.top, -systemUiOverlap.top),
                Math.max(viewportOffsetRect.right, systemUiOverlap.right),
                Math.max(viewportOffsetRect.bottom, systemUiOverlap.bottom));
    }

    /**
     * Provides the amount of padding needed to center the image content on the screen.
     */
    private PointF getLetterboxPadding() {
        float[] imageVectors = {mRenderData.imageWidth, mRenderData.imageHeight};
        mRenderData.transform.mapVectors(imageVectors);

        // We want to letterbox when the image is smaller than the screen in a specific dimension.
        // Since we center the image, split the difference so it is equally distributed.
        float widthAdjust = Math.max((getSafeScreenWidth() - imageVectors[0]) / 2, 0);
        float heightAdjust = Math.max((getAdjustedScreenHeight() - imageVectors[1]) / 2, 0);

        return new PointF(widthAdjust, heightAdjust);
    }

    /**
     * Returns the amount of System UI along each edge of the screen which could overlap the remote
     * desktop image below it.  This is the maximum amount that could overlap, not the actual value.
     */
    private RectF getSystemUiOverlap() {
        if (mSystemUiScreenRect.isEmpty()) {
            return new RectF();
        }

        // Letterbox padding represents the space added to the image to center it on the screen.
        // Since it does not contain any interactable UI, we ignore it when calculating the overlap
        // between the System UI and the remote desktop image.
        // Note: Ignore negative padding (clamp to 0) since that means no overlap exists.
        float adjustedScreenHeight = getAdjustedScreenHeight();
        PointF letterboxPadding = getLetterboxPadding();
        return new RectF(Math.max(mSystemUiScreenRect.left - letterboxPadding.x, 0.0f),
                Math.max(mSystemUiScreenRect.top - letterboxPadding.y, 0.0f),
                Math.max(mRenderData.screenWidth - mSystemUiScreenRect.right - letterboxPadding.x,
                        0.0f),
                Math.max(adjustedScreenHeight - mSystemUiScreenRect.bottom - letterboxPadding.y,
                        0.0f));
    }

    /**
     * Applies the given offset, as needed, to allow moving the image outside its normal bounds.
     */
    private void calculateViewportOffset(float offsetX, float offsetY) {
        if (mSystemUiScreenRect.isEmpty()) {
            // We only want to directly change the viewport offset when System UI is present.
            return;
        }

        float[] offsets = {offsetX, offsetY};
        mRenderData.transform.mapVectors(offsets);

        // Use a temporary variable here as {@link #getViewportOffsetBounds()} uses the current
        // viewport offset as a maximum boundary.  If we add the offset first, the result ends up
        // being unbounded.  Thus we use a temporary object for the boundary calculation.
        PointF requestedOffset =
                new PointF(mViewportOffset.x + offsets[0], mViewportOffset.y + offsets[1]);
        constrainPointToBounds(requestedOffset, getViewportOffsetBounds());
        mViewportOffset.set(requestedOffset);
    }

    /**
     * Starts an animation to smoothly reduce the viewport offset.  Does nothing if an animation is
     * already running, the offset is already 0, or the offset and target are the same.
     */
    private void startOffsetReductionAnimation(final PointF targetOffset) {
        if (mFrameRenderedCallback != null || mViewportOffset.length() < EPSILON
                || arePointsEqual(mViewportOffset, targetOffset, EPSILON)) {
            return;
        }

        mFrameRenderedCallback = new Event.ParameterCallback<Boolean, Void>() {
            private static final float DURATION_MS = 250.0f;

            private final Interpolator mInterpolator = new DecelerateInterpolator();

            private long mStartTime;
            private final float mOriginalX = mViewportOffset.x - targetOffset.x;
            private final float mOriginalY = mViewportOffset.y - targetOffset.y;

            @Override
            public Boolean run(Void p) {
                if (mFrameRenderedCallback == null) {
                    return false;
                }

                if (mStartTime == 0) {
                    mStartTime = SystemClock.elapsedRealtime();
                }

                float progress = (SystemClock.elapsedRealtime() - mStartTime) / DURATION_MS;
                if (progress < 1.0f) {
                    float reductionFactor = 1.0f - mInterpolator.getInterpolation(progress);
                    mViewportOffset.set(mOriginalX * reductionFactor + targetOffset.x,
                            mOriginalY * reductionFactor + targetOffset.y);
                } else {
                    mViewportOffset.set(targetOffset.x, targetOffset.y);
                    mFrameRenderedCallback = null;
                }

                repositionImage();

                return mFrameRenderedCallback != null;
            }
        };

        mRenderStub.onCanvasRendered().addSelfRemovable(mFrameRenderedCallback);
    }

    /**
     * Stops an existing offset reduction animation.
     */
    private void stopOffsetReductionAnimation() {
        // Setting this value this null will prevent it from continuing to execute.
        mFrameRenderedCallback = null;
    }

    private boolean arePointsEqual(PointF a, PointF b, float epsilon) {
        return Math.abs(a.x - b.x) < epsilon && Math.abs(a.y - b.y) < epsilon;
    }

    /**
     * @return Screen width inset by {@link #mSafeInsets} if {@link #mSystemUiScreenRect} is empty,
     * otherwise just the screen width.
     */
    private float getSafeScreenWidth() {
        if (!mSystemUiScreenRect.isEmpty()) {
            return mRenderData.screenWidth;
        }
        return mRenderData.screenWidth - mSafeInsets.left - mSafeInsets.right;
    }

    /**
     * @return Screen height inset by {@link #mSafeInsets} if {@link #mSystemUiScreenRect} is empty,
     * otherwise just the screen height.
     */
    private float getSafeScreenHeight() {
        if (!mSystemUiScreenRect.isEmpty()) {
            return mRenderData.screenHeight;
        }
        return mRenderData.screenHeight - mSafeInsets.top - mSafeInsets.bottom;
    }

    /**
     * @return Center point of the screen area inset by {@link #mSafeInsets} if {@link
     * #mSystemUiScreenRect} is empty, otherwise just the center of the screen.
     */
    private PointF getSafeScreenCenterPoint() {
        if (!mSystemUiScreenRect.isEmpty()) {
            return new PointF(mRenderData.screenWidth / 2.f, mRenderData.screenHeight / 2.f);
        }
        return new PointF((mRenderData.screenWidth + mSafeInsets.left - mSafeInsets.right) / 2.f,
                (mRenderData.screenHeight + mSafeInsets.top - mSafeInsets.bottom) / 2.f);
    }

    /**
     * @return Same as {@link #getSafeScreenCenterPoint()} if {@link #mSystemUiScreenRect} is empty,
     * otherwise return (screenWidth / 2, adjustedScreenHeight / 2).
     */
    private PointF getAdjustedScreenCenterPoint() {
        if (mSystemUiScreenRect.isEmpty()) {
            return getSafeScreenCenterPoint();
        }
        return new PointF(mRenderData.screenWidth / 2.f, getAdjustedScreenHeight() / 2.f);
    }
}
