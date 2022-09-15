// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.util.Pair;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ViewConfiguration;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for handling Touch input from the user.  Touch events which manipulate
 * the local canvas are handled in this class and any input which should be sent to the remote host
 * are passed to the InputStrategyInterface implementation set by the DesktopView.
 */
public class TouchInputHandler {
    private static final float EPSILON = 0.001f;

    private final List<Pair<Object, Event<?>>> mAttachedEvents = new ArrayList<>();
    private final Desktop mDesktop;
    private final RenderData mRenderData;
    private final DesktopCanvas mDesktopCanvas;
    private final RenderStub mRenderStub;
    private final GestureDetector mScroller;
    private final ScaleGestureDetector mZoomer;
    private final TapGestureDetector mTapDetector;

    /** Used to disambiguate a 2-finger gesture as a swipe or a pinch. */
    private final SwipePinchDetector mSwipePinchDetector;

    // Used for processing cursor & scroller fling animations.
    // May consider using a List of AnimationJob if we have more than two animation jobs in
    // the future.
    private final FlingAnimationJob mCursorAnimationJob;
    private final FlingAnimationJob mScrollAnimationJob;

    private InputStrategyInterface mInputStrategy;

    /**
     * Used for tracking swipe gestures. Only the Y-direction is needed for responding to swipe-up
     * or swipe-down.
     */
    private float mTotalMotionY;

    /**
     * Distance in pixels beyond which a motion gesture is considered to be a swipe. This is
     * initialized using the Context passed into the ctor.
     */
    private float mSwipeThreshold;

    /**
     * Distance, in pixels, from the edge of the screen in which a touch event should be considered
     * as having originated from that edge.
     */
    private final int mEdgeSlopInPx;

    /**
     * Defines an inset boundary within which pan gestures are allowed.  Pan gestures which
     * originate outside of this boundary will be ignored.
     */
    private Rect mPanGestureBounds = new Rect();

    /**
     * Set to true to prevent any further movement of the cursor, for example, when showing the
     * keyboard to prevent the cursor wandering from the area where keystrokes should be sent.
     */
    private boolean mSuppressCursorMovement;

    /**
     * Set to true to suppress the fling animation at the end of a gesture, for example, when
     * dragging whilst a button is held down.
     */
    private boolean mSuppressFling;

    /**
     * Set to true when 2-finger fling (scroll gesture with final velocity) is detected to trigger
     * a scrolling animation.
     */
    private boolean mScrollFling;

    /**
     * Set to true when 3-finger swipe gesture is complete, so that further movement doesn't
     * trigger more swipe actions.
     */
    private boolean mSwipeCompleted;

    /**
     * Set to true when a 1 finger pan gesture originates with a longpress.  This means the user
     * is performing a drag operation.
     */
    private boolean mIsDragging;

    private Event.ParameterCallback<Boolean, Void> mProcessAnimationCallback;

    /**
     * This class implements fling animation for cursor
     */
    private class CursorAnimationJob extends FlingAnimationJob {
        public CursorAnimationJob(Context context) {
            super(context);
        }

        @Override
        protected void processAction(float deltaX, float deltaY) {
            float[] delta = {deltaX, deltaY};
            Matrix canvasToImage = new Matrix();
            mRenderData.transform.invert(canvasToImage);
            canvasToImage.mapVectors(delta);

            moveViewportByOffset(-delta[0], -delta[1]);
        }
    }

    /**
     * This class implements fling animation for scrolling
     */
    private class ScrollAnimationJob extends FlingAnimationJob {
        public ScrollAnimationJob(Context context) {
            super(context);
        }

        @Override
        protected void processAction(float deltaX, float deltaY) {
            mInputStrategy.onScroll(-deltaX, -deltaY);
        }
    }

    /**
     * This class provides a NULL implementation which will be used until a real input
     * strategy has been created and set.  Using this as the default implementation will prevent
     * crashes if the owning class does not create/set a real InputStrategy before the host size
     * information is received (or if the user interacts with the screen in that case).  This class
     * has default values which will also allow the user to pan/zoom the desktop image until an
     * InputStrategy implementation has been set.
     */
    private static class NullInputStrategy implements InputStrategyInterface {
        NullInputStrategy() {}

        @Override
        public boolean onTap(int button) {
            return false;
        }

        @Override
        public boolean onPressAndHold(int button) {
            return false;
        }

        @Override
        public void onMotionEvent(MotionEvent event) {
            return;
        }

        @Override
        public void onScroll(float distanceX, float distanceY) {
            return;
        }

        @Override
        public void injectCursorMoveEvent(int x, int y) {
            return;
        }

        @Override
        public @RenderStub.InputFeedbackType int getShortPressFeedbackType() {
            return RenderStub.InputFeedbackType.NONE;
        }

        @Override
        public @RenderStub.InputFeedbackType int getLongPressFeedbackType() {
            return RenderStub.InputFeedbackType.NONE;
        }

        @Override
        public boolean isIndirectInputMode() {
            return false;
        }
    }

    public TouchInputHandler(DesktopView viewer, Desktop desktop, RenderStub renderStub,
                             final InputEventSender injector) {
        Preconditions.notNull(viewer);
        Preconditions.notNull(desktop);
        Preconditions.notNull(renderStub);
        Preconditions.notNull(injector);

        mDesktop = desktop;
        mRenderStub = renderStub;
        mRenderData = new RenderData();
        mDesktopCanvas = new DesktopCanvas(renderStub, mRenderData);

        GestureListener listener = new GestureListener();
        mScroller = new GestureDetector(desktop, listener, null, false);

        // If long-press is enabled, the gesture-detector will not emit any further onScroll
        // notifications after the onLongPress notification. Since onScroll is being used for
        // moving the cursor, it means that the cursor would become stuck if the finger were held
        // down too long.
        mScroller.setIsLongpressEnabled(false);

        mZoomer = new ScaleGestureDetector(desktop, listener);
        mTapDetector = new TapGestureDetector(desktop, listener);
        mSwipePinchDetector = new SwipePinchDetector(desktop);

        // The threshold needs to be bigger than the ScaledTouchSlop used by the gesture-detectors,
        // so that a gesture cannot be both a tap and a swipe. It also needs to be small enough so
        // that intentional swipes are usually detected.
        float density = desktop.getResources().getDisplayMetrics().density;
        mSwipeThreshold = 40 * density;

        mEdgeSlopInPx = ViewConfiguration.get(desktop).getScaledEdgeSlop();

        mInputStrategy = new NullInputStrategy();

        mCursorAnimationJob = new CursorAnimationJob(desktop);
        mScrollAnimationJob = new ScrollAnimationJob(desktop);

        mProcessAnimationCallback = new Event.ParameterCallback<Boolean, Void>() {
            @Override
            public Boolean run(Void p) {
                return processAnimation();
            }
        };

        attachEvent(viewer.onTouch(), new Event.ParameterRunnable<TouchEventParameter>() {
            @Override
            public void run(TouchEventParameter parameter) {
                parameter.handled = handleTouchEvent(parameter.event);
            }
        });

        attachEvent(desktop.onInputModeChanged(),
                new Event.ParameterRunnable<InputModeChangedEventParameter>() {
                    @Override
                    public void run(InputModeChangedEventParameter parameter) {
                        handleInputModeChanged(parameter, injector);
                    }
                });

        attachEvent(desktop.onSystemUiVisibilityChanged(),
                new Event.ParameterRunnable<SystemUiVisibilityChangedEventParameter>() {
                    @Override
                    public void run(SystemUiVisibilityChangedEventParameter parameter) {
                        mDesktopCanvas.onSystemUiVisibilityChanged(parameter);
                    }
                });

        attachEvent(renderStub.onClientSizeChanged(),
                new Event.ParameterRunnable<SizeChangedEventParameter>() {
                    @Override
                    public void run(SizeChangedEventParameter parameter) {
                        handleClientSizeChanged(parameter.width, parameter.height);
                    }
                });

        attachEvent(renderStub.onHostSizeChanged(),
                new Event.ParameterRunnable<SizeChangedEventParameter>() {
                    @Override
                    public void run(SizeChangedEventParameter parameter) {
                        handleHostSizeChanged(parameter.width, parameter.height);
                    }
                });
    }

    private <ParamT> void attachEvent(Event<ParamT> event,
                                      Event.ParameterRunnable<ParamT> runnable) {
        mAttachedEvents.add(new Pair<Object, Event<?>>(event.add(runnable), event));
    }

    /**
     * Detaches all registered event listeners. This function should be called exactly once.
     */
    public void detachEventListeners() {
        Preconditions.isTrue(!mAttachedEvents.isEmpty());
        abortAnimation();
        for (Pair<Object, Event<?>> pair : mAttachedEvents) {
            pair.second.remove(pair.first);
        }
        mAttachedEvents.clear();
    }

    /**
     * Steps forward the animation.
     * @return true if the animation is not finished yet.
     */
    private boolean processAnimation() {
        return mCursorAnimationJob.processAnimation() || mScrollAnimationJob.processAnimation();
    }

    /**
     * Start stepping animation when onCanvasRendered is triggered.
     */
    private void startAnimation() {
        mRenderStub.onCanvasRendered().addSelfRemovable(mProcessAnimationCallback);
    }

    /**
     * Abort all animations.
     */
    private void abortAnimation() {
        mCursorAnimationJob.abortAnimation();
        mScrollAnimationJob.abortAnimation();
    }

    private void handleInputModeChanged(
            InputModeChangedEventParameter parameter, InputEventSender injector) {
        final @Desktop.InputMode int inputMode = parameter.inputMode;
        final @CapabilityManager.HostCapability int hostTouchCapability = parameter.hostCapability;
        // We need both input mode and host input capabilities to select the input
        // strategy.
        if (!CapabilityManager.hostCapabilityIsSet(inputMode)
                || !CapabilityManager.hostCapabilityIsSet(hostTouchCapability)) {
            return;
        }

        switch (inputMode) {
            case Desktop.InputMode.TRACKPAD:
                setInputStrategy(new TrackpadInputStrategy(mRenderData, injector));
                mDesktopCanvas.adjustViewportForSystemUi(true);
                moveCursorToScreenCenter();
                break;

            case Desktop.InputMode.TOUCH:
                mDesktopCanvas.adjustViewportForSystemUi(false);
                if (CapabilityManager.hostCapabilityIsSupported(hostTouchCapability)) {
                    setInputStrategy(new TouchInputStrategy(mRenderData, injector));
                } else {
                    setInputStrategy(
                            new SimulatedTouchInputStrategy(mRenderData, injector, mDesktop));
                }
                break;

            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
        }

        // Ensure the cursor state is updated appropriately.
        mRenderStub.setCursorVisibility(mRenderData.drawCursor);
    }

    private boolean handleTouchEvent(MotionEvent event) {
        // Give the underlying input strategy a chance to observe the current motion event before
        // passing it to the gesture detectors.  This allows the input strategy to react to the
        // event or save the payload for use in recreating the gesture remotely.
        mInputStrategy.onMotionEvent(event);

        // Avoid short-circuit logic evaluation - ensure all gesture detectors see all events so
        // that they generate correct notifications.
        boolean handled = mScroller.onTouchEvent(event);
        handled |= mZoomer.onTouchEvent(event);
        handled |= mTapDetector.onTouchEvent(event);
        mSwipePinchDetector.onTouchEvent(event);

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                abortAnimation();
                mSuppressCursorMovement = false;
                mSuppressFling = false;
                mSwipeCompleted = false;
                mIsDragging = false;
                break;

            case MotionEvent.ACTION_POINTER_DOWN:
                mTotalMotionY = 0;
                break;

            default:
                break;
        }
        return handled;
    }

    private void handleClientSizeChanged(int width, int height) {
        mRenderData.screenWidth = width;
        mRenderData.screenHeight = height;

        mDesktopCanvas.setSafeInsets(mDesktop.getSafeInsets());

        mPanGestureBounds = new Rect(
                mEdgeSlopInPx, mEdgeSlopInPx, width - mEdgeSlopInPx, height - mEdgeSlopInPx);
        resizeImageToFitScreen();

        mDesktop.sendPreferredHostResolution();
    }

    private void handleHostSizeChanged(int width, int height) {
        mRenderData.imageWidth = width;
        mRenderData.imageHeight = height;

        resizeImageToFitScreen();
    }

    private void resizeImageToFitScreen() {
        if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0
                || mRenderData.screenWidth == 0 || mRenderData.screenHeight == 0) {
            return;
        }

        mDesktopCanvas.resizeImageToFitScreen();

        moveCursorToScreenCenter();
    }

    private void moveCursorToScreenCenter() {
        float screenCenterX = (float) mRenderData.screenWidth / 2;
        float screenCenterY = (float) mRenderData.screenHeight / 2;

        float[] imagePoint = mapScreenPointToImagePoint(screenCenterX, screenCenterY);
        mDesktopCanvas.setCursorPosition(imagePoint[0], imagePoint[1]);

        moveCursorToScreenPoint(screenCenterX, screenCenterY);
    }

    private void setInputStrategy(InputStrategyInterface inputStrategy) {
        // Since the rules for flinging differ between input modes, we want to stop running the
        // current fling animation when the mode changes to prevent a wonky experience.
        abortAnimation();
        mInputStrategy = inputStrategy;
    }

    /** Moves the desired center of the viewport using the specified deltas. */
    private void moveViewportByOffset(float deltaX, float deltaY) {
        // If we are in an indirect mode or are in the middle of a drag operation, then we want to
        // invert the direction of the operation (i.e. follow the motion of the finger).
        boolean followCursor = (mInputStrategy.isIndirectInputMode() || mIsDragging);
        if (followCursor) {
            deltaX = -deltaX;
            deltaY = -deltaY;
        }

        // Determine the center point from which to apply the delta.
        // For indirect input modes (i.e. trackpad), the view generally follows the cursor.
        // For direct input modes (i.e. touch) the should track the user's motion.
        // If the user is dragging, then the viewport should always follow the user's finger.
        if (mInputStrategy.isIndirectInputMode()) {
            PointF newCursorPos = mDesktopCanvas.moveCursorPosition(deltaX, deltaY);
            moveCursor(newCursorPos.x, newCursorPos.y);
        } else {
            mDesktopCanvas.moveViewportCenter(deltaX, deltaY);
        }
    }

    /** Moves the cursor to the specified position on the screen. */
    private void moveCursorToScreenPoint(float screenX, float screenY) {
        float[] imagePoint = mapScreenPointToImagePoint(screenX, screenY);
        moveCursor(imagePoint[0], imagePoint[1]);
    }

    /** Moves the cursor to the specified position on the remote host. */
    private void moveCursor(float newX, float newY) {
        boolean cursorMoved = mRenderData.setCursorPosition(newX, newY);
        if (cursorMoved) {
            mInputStrategy.injectCursorMoveEvent((int) newX, (int) newY);
        }

        mRenderStub.moveCursor(mRenderData.getCursorPosition());
    }

    /** Processes a (multi-finger) swipe gesture. */
    private boolean onSwipe() {
        if (mTotalMotionY > mSwipeThreshold) {
            // Swipe down occurred.
            mDesktop.showSystemUi();
        } else if (mTotalMotionY < -mSwipeThreshold) {
            // Swipe up occurred.
            mDesktop.showKeyboard();
        } else {
            return false;
        }

        mSuppressCursorMovement = true;
        mSuppressFling = true;
        mSwipeCompleted = true;
        return true;
    }

    /** Translates a point in screen coordinates to a location on the desktop image. */
    private float[] mapScreenPointToImagePoint(float screenX, float screenY) {
        float[] mappedPoints = {screenX, screenY};
        Matrix screenToImage = new Matrix();

        mRenderData.transform.invert(screenToImage);

        screenToImage.mapPoints(mappedPoints);

        return mappedPoints;
    }

    /** Responds to touch events filtered by the gesture detectors. */
    private class GestureListener extends GestureDetector.SimpleOnGestureListener
            implements ScaleGestureDetector.OnScaleGestureListener,
                       TapGestureDetector.OnTapListener {
        /**
         * Called when the user drags one or more fingers across the touchscreen.
         */
        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            int pointerCount = e2.getPointerCount();

            // Check to see if the motion originated at the edge of the screen.
            // If so, then the user is likely swiping in to display system UI.
            if (!mPanGestureBounds.contains((int) e1.getX(), (int) e1.getY())) {
                // Prevent the cursor being moved or flung by the gesture.
                mSuppressCursorMovement = true;
                return false;
            }

            if (pointerCount >= 3 && !mSwipeCompleted) {
                // Note that distance values are reversed. For example, dragging a finger in the
                // direction of increasing Y coordinate (downwards) results in distanceY being
                // negative.
                mTotalMotionY -= distanceY;
                return onSwipe();
            }

            if (pointerCount == 2 && mSwipePinchDetector.isSwiping()) {
                if (!mInputStrategy.isIndirectInputMode()) {
                    // Ensure the cursor is located at the coordinates of the original event,
                    // otherwise the target window may not receive the scroll event correctly.
                    moveCursorToScreenPoint(e1.getX(), e1.getY());
                }
                mInputStrategy.onScroll(distanceX, distanceY);

                // Prevent the cursor being moved or flung by the gesture.
                mSuppressCursorMovement = true;
                mScrollFling = true;
                return true;
            }

            if (pointerCount != 1 || mSuppressCursorMovement) {
                return false;
            }

            float[] delta = {distanceX, distanceY};

            Matrix canvasToImage = new Matrix();
            mRenderData.transform.invert(canvasToImage);
            canvasToImage.mapVectors(delta);

            moveViewportByOffset(delta[0], delta[1]);
            if (!mInputStrategy.isIndirectInputMode() && mIsDragging) {
                // Ensure the cursor follows the user's finger when the user is dragging under
                // direct input mode.
                moveCursorToScreenPoint(e2.getX(), e2.getY());
            }
            return true;
        }

        /**
         * Called when a fling gesture is recognized.
         */
        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            if (mSuppressFling) {
                return false;
            }

            if (mScrollFling) {
                mScrollAnimationJob.startAnimation(velocityX, velocityY);
                startAnimation();
                mScrollFling = false;
                return true;
            }

            if (mSuppressCursorMovement) {
                return false;
            }

            // If cursor movement is suppressed, fling also needs to be suppressed, as the
            // gesture-detector will still generate onFling() notifications based on movement of
            // the fingers, which would result in unwanted cursor movement.
            mCursorAnimationJob.startAnimation(velocityX, velocityY);
            startAnimation();
            return true;
        }

        /** Called when the user is in the process of pinch-zooming. */
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (!mSwipePinchDetector.isPinching()) {
                return false;
            }

            mDesktopCanvas.scaleAndRepositionImage(detector.getScaleFactor(), detector.getFocusX(),
                    detector.getFocusY(), mInputStrategy.isIndirectInputMode());

            return true;
        }

        /** Called whenever a gesture starts. Always accepts the gesture so it isn't ignored. */
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }

        /**
         * Called when the user starts to zoom. Always accepts the zoom so that
         * onScale() can decide whether to respond to it.
         */
        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            return true;
        }

        /** Called when the user is done zooming. Defers to onScale()'s judgement. */
        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            onScale(detector);
        }

        /** Called when the user taps the screen with one or more fingers. */
        @Override
        public boolean onTap(int pointerCount, float x, float y) {
            int button = mouseButtonFromPointerCount(pointerCount);
            if (button == InputStub.BUTTON_UNDEFINED) {
                return false;
            }

            if (!mInputStrategy.isIndirectInputMode()) {
                if (screenPointLiesOutsideImageBoundary(x, y)) {
                    return false;
                }
                moveCursorToScreenPoint(x, y);
            }

            if (mInputStrategy.onTap(button)) {
                PointF pos = mRenderData.getCursorPosition();

                mRenderStub.showInputFeedback(mInputStrategy.getShortPressFeedbackType(), pos);
            }
            return true;
        }

        /** Called when a long-press is triggered for one or more fingers. */
        @Override
        public void onLongPress(int pointerCount, float x, float y) {
            int button = mouseButtonFromPointerCount(pointerCount);
            if (button == InputStub.BUTTON_UNDEFINED) {
                return;
            }

            if (!mInputStrategy.isIndirectInputMode()) {
                if (screenPointLiesOutsideImageBoundary(x, y)) {
                    return;
                }
                moveCursorToScreenPoint(x, y);
            }

            if (mInputStrategy.onPressAndHold(button)) {
                PointF pos = mRenderData.getCursorPosition();

                mRenderStub.showInputFeedback(mInputStrategy.getLongPressFeedbackType(), pos);
                mSuppressFling = true;
                mIsDragging = true;
            }
        }

        /** Maps the number of fingers in a tap or long-press gesture to a mouse-button. */
        private int mouseButtonFromPointerCount(int pointerCount) {
            switch (pointerCount) {
                case 1:
                    return InputStub.BUTTON_LEFT;
                case 2:
                    return InputStub.BUTTON_RIGHT;
                case 3:
                    return InputStub.BUTTON_MIDDLE;
                default:
                    return InputStub.BUTTON_UNDEFINED;
            }
        }

        /** Determines whether the given screen point lies outside the desktop image. */
        private boolean screenPointLiesOutsideImageBoundary(float screenX, float screenY) {
            float[] mappedPoints = mapScreenPointToImagePoint(screenX, screenY);

            float imageWidth = (float) mRenderData.imageWidth + EPSILON;
            float imageHeight = (float) mRenderData.imageHeight + EPSILON;

            return mappedPoints[0] < -EPSILON || mappedPoints[0] > imageWidth
                    || mappedPoints[1] < -EPSILON || mappedPoints[1] > imageHeight;
        }
    }
}
