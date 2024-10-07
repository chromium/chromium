/*
 * Copyright 2024 The Android Open Source Project
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

package androidx.pdf.viewer;

import android.content.Context;
import android.graphics.Rect;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.annotation.VisibleForTesting;
import androidx.core.os.ParcelCompat;
import androidx.pdf.ViewState;
import androidx.pdf.data.Range;
import androidx.pdf.util.PaginationUtils;
import androidx.pdf.util.Preconditions;
import androidx.pdf.util.ThreadUtils;
import androidx.pdf.viewer.PageViewFactory.PageView;
import androidx.pdf.viewer.loader.PdfLoader;
import androidx.pdf.widget.ZoomView;

import java.util.AbstractList;
import java.util.List;

/**
 * View to display the PDF pages assembled in a vertical strip by {@code #model}.
 *
 * <p>{@code #model} may hold many more pages than actually fit at any time on the screen. This view
 * will operate by having just a handful of pages actually instantiated into Views at any time.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
@SuppressWarnings("WrongCall")
public class PaginatedView extends ViewGroup implements PaginationModelObserver {
    /** Maps the current child views to pages. */
    private final SparseArray<PageView> mPageViews = new SparseArray<>();

    private PaginationModel mModel;

    private PageRangeHandler mPageRangeHandler;

    private PdfSelectionModel mSelectionModel;

    private PdfSelectionHandles mSelectionHandles;

    private SearchModel mSearchModel;

    private PdfLoader mPdfLoader;

    private PageViewFactory mPageViewFactory;

    private boolean mIsConfigurationChanged = false;

    /** The current viewport in content coordinates */
    private final Rect mViewArea = new Rect();

    public PaginatedView(@NonNull Context context) {
        this(context, null);
    }

    public PaginatedView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public PaginatedView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mModel = new PaginationModel(context);
        mPageRangeHandler = new PageRangeHandler(mModel);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mModel.addObserver(this);
    }


    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mModel.removeObserver(this);
    }

    @VisibleForTesting
    public void setModel(@NonNull PaginationModel model) {
        mModel = model;
    }

    @NonNull
    public PaginationModel getModel() {
        return mModel;
    }

    @NonNull
    public PaginationModel resetModels() {
        mModel = new PaginationModel(getContext());
        mPageRangeHandler = new PageRangeHandler(mModel);
        return mModel;
    }

    /** Requests a layout because this view has to grow now to accommodate the new page(s). */
    @Override
    public void onPageAdded() {
        requestLayout();
    }

    protected boolean isInitialized() {
        return mModel != null;
    }

    /**
     * Measures this view in relation to the {@link #mModel} then asks all child views to measure
     * themselves.
     *
     * <p>If the {@link #mModel} is not initialized, this view has nothing to display and will
     * measure (0, 0). Otherwise, view will measure ({@link #mModel}'s width, {@link #mModel}'s
     * estimated height).
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = 0;
        int estimatedHeight = 0;

        if (isInitialized()) {
            width = mModel.getWidth();
            estimatedHeight = mModel.getEstimatedFullHeight();
        }

        setMeasuredDimension(width, estimatedHeight);
        measureChildren(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Provides consistent layout behavior for subclasses.
     *
     * <p>Does not perform a layout if there aren't any child views. Otherwise asks the
     * subclasses to
     * layout each child by index.
     */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int count = getChildCount();
        if (count == 0) {
            return;
        }

        for (int i = 0; i < count; i++) {
            layoutChild(i);
        }
    }

    @Nullable
    @Override
    protected Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        return new SavedState(superState, mModel);
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        SavedState savedState = (SavedState) state;
        super.onRestoreInstanceState(((SavedState) state).getSuperState());
        mModel = savedState.mModel;
        mPageRangeHandler = new PageRangeHandler(mModel);
        requestLayout();
    }

    /**
     * Returns the current viewport in content coordinates
     */
    @NonNull
    public Rect getViewArea() {
        return mViewArea;
    }

    /**
     * Updates the current viewport
     *
     * @param viewArea the viewport in content coordinates
     */
    public void setViewArea(@NonNull Rect viewArea) {
        if (!viewArea.equals(this.mViewArea)) {
            this.mViewArea.set(viewArea);
            onViewAreaChanged();
        }
    }

    @NonNull
    public PageRangeHandler getPageRangeHandler() {
        return mPageRangeHandler;
    }

    @NonNull
    public PdfSelectionModel getSelectionModel() {
        return mSelectionModel;
    }

    public void setSelectionModel(
            @NonNull PdfSelectionModel selectionModel) {
        mSelectionModel = selectionModel;
    }

    @NonNull
    public SearchModel getSearchModel() {
        return mSearchModel;
    }

    public void setSearchModel(@NonNull SearchModel searchModel) {
        mSearchModel = searchModel;
    }

    @NonNull
    public PdfSelectionHandles getSelectionHandles() {
        return mSelectionHandles;
    }

    public void setSelectionHandles(@NonNull PdfSelectionHandles selectionHandles) {
        mSelectionHandles = selectionHandles;
    }

    public void setPdfLoader(@NonNull PdfLoader pdfLoader) {
        mPdfLoader = pdfLoader;
    }

    @NonNull
    public PageViewFactory getPageViewFactory() {
        return mPageViewFactory;
    }

    public void setPageViewFactory(@NonNull PageViewFactory pageViewFactory) {
        mPageViewFactory = pageViewFactory;
    }

    /** Instantiate a page of this pageView into a child pageView. */
    public void addView(@NonNull PageView pageView) {
        int pageNum = pageView.getPageNum();
        Preconditions.checkState(pageNum < getModel().getSize(),
                "Can't add pageView for unknown page");
        mPageViews.put(pageNum, pageView);
        View view = pageView.asView();
        if (mPageViews.size() == 1) {
            super.addView(view);
        } else {
            int index = mPageViews.indexOfKey(pageNum);
            if (index < mPageViews.size() - 1) {
                super.addView(view, index);
            } else {
                super.addView(view);
            }
        }
    }

    @Override
    public void addView(View child) {
        throw new UnsupportedOperationException("Not supported - Use addPage instead");
    }

    @Override
    public void addView(View child, int width, int height) {
        throw new UnsupportedOperationException("Not supported - Use addPage instead");
    }

    @Override
    public void addView(View child, android.view.ViewGroup.LayoutParams params) {
        throw new UnsupportedOperationException("Not supported - Use addPage instead");
    }

    /** Return the view of the given page number. */
    @Nullable
    public PageView getViewAt(int pageNum) {
        return mPageViews.get(pageNum);
    }

    /**
     * Returns an unmodifiable list with pages that are currently instantiated as Views.
     *
     * <p>The list is backed by this view and will likely change soon, so is only suitable for
     * immediate iteration.
     */
    @NonNull
    public List<PageMosaicView> getChildViews() {
        return new AbstractList<PageMosaicView>() {

            @Override
            public PageMosaicView get(int index) {
                return ((PageView) getChildAt(index)).getPageView();
            }

            @Override
            public int size() {
                return getChildCount();
            }
        };
    }

    /** Can only clear first or last view. */
    @Override
    public void removeViewAt(int pageNum) {
        int index = mPageViews.indexOfKey(pageNum);
        if (index < 0) {
            return;
        }

        PageView page = getViewAt(pageNum);
        if (page != null) {
            mPageViews.delete(pageNum);
            removeView(page.asView());
            page.clearAll();
        }
    }

    @Override
    public void removeAllViews() {
        if (mPageRangeHandler != null) {
            mPageRangeHandler.setVisiblePages(null);
        }

        for (int i = 0; i < mPageViews.size(); i++) {
            mPageViews.valueAt(i).clearAll();
        }
        super.removeAllViews();
        mPageViews.clear();
    }

    /**
     * Issues the {@link #layout} call for one child view.
     *
     * <p>The child view for page X will be laid out in bound's that match page X's size in the
     * {@link
     * PaginationModel} exactly. The child will be positioned according to {@link
     * PaginationModel#getPageLocation} which positions the page:
     *
     * <ul>
     *   <li>vertically at the given <code>top</code> coordinates (between top and top + height),
     *   <li>horizontally between <code>0</code> and {@link PaginationModel#getWidth()} if possible,
     *       in a way that maximizes the portion of that view that is visible on the screen
     * </ul>
     *
     * @param index the index of the child view in this ViewGroup
     */
    private void layoutChild(int index) {
        int pageNum = mPageViews.keyAt(index);
        Rect viewArea = getViewArea();
        Rect pageCoordinates = getModel().getPageLocation(pageNum, viewArea);

        PageView child = (PageView) getChildAt(index);
        child
                .asView()
                .layout(
                        pageCoordinates.left,
                        pageCoordinates.top,
                        pageCoordinates.right,
                        pageCoordinates.bottom);

        child
                .getPageView()
                .setViewArea(
                        viewArea.left - pageCoordinates.left,
                        viewArea.top - pageCoordinates.top,
                        viewArea.right - pageCoordinates.left,
                        viewArea.bottom - pageCoordinates.top);
    }

    /** Clear all highlight overlays. */
    public void clearAllOverlays() {
        for (PageMosaicView view : getChildViews()) {
            view.setOverlay(null);
        }
    }

    /** Perform a layout when the viewArea of the {@code model} has changed. */
    private void onViewAreaChanged() {
        // We can't wait for the next layout pass, the pages will be drawn before.
        // We could still optimize to skip the next layoutChild() calls for the pages that have been
        // laid out already for this viewArea.
        onLayout(false, getLeft(), getTop(), getRight(), getBottom());
    }

    /**
     * Refreshes the page range for the visible area.
     */
    public void refreshPageRangeInVisibleArea(@NonNull ZoomView.ZoomScroll zoomScroll,
            int parentViewHeight) {
        mPageRangeHandler.refreshVisiblePageRange(zoomScroll.scrollY, zoomScroll.zoom,
                parentViewHeight);

        mPageRangeHandler.adjustMaxPageToUpperVisibleRange();
    }

    /** Cancels the background jobs for the disappeared pages and optionally clears the views */
    public void handleGonePages(boolean clearViews) {
        Range nearPages = mPageRangeHandler.getNearPagesToVisibleRange();
        Range[] gonePages = mPageRangeHandler.getGonePageRanges(nearPages);
        for (Range pages : gonePages) {
            // Keep Views around for now, we'll clear them in step (4) if applicable.
            clearPages(pages, clearViews);
        }
    }

    /** Computes the invisible page range and loads them */
    public void loadInvisibleNearPageRange(
            float stableZoom) {
        Range nearPages = mPageRangeHandler.getNearPagesToVisibleRange();
        Range[] invisibleNearPages = mPageRangeHandler.getInvisibleNearPageRanges(nearPages);

        for (Range pages : invisibleNearPages) {
            loadPageRange(pages, stableZoom);
        }
    }

    /**
     * Creates the page views for the visible page range.
     *
     * @return true if any new page was created else false
     */
    public boolean createPageViewsForVisiblePageRange() {
        boolean requiresLayoutPass = false;
        for (int pageNum : mPageRangeHandler.getVisiblePages()) {
            if (getViewAt(pageNum) == null) {
                mPageViewFactory.getOrCreatePageView(pageNum,
                        PaginationUtils.getPageElevationInPixels(getContext()),
                        mModel.getPageSize(pageNum));
                requiresLayoutPass = true;
            }
        }
        return requiresLayoutPass;
    }

    /**  */
    public void refreshVisiblePages(boolean requiresLayoutPass,
            @NonNull ViewState viewState,
            float stableZoom) {
        if (requiresLayoutPass) {
            refreshPagesAfterLayout(viewState, mPageRangeHandler.getVisiblePages(),
                    stableZoom);
        } else {
            refreshPages(mPageRangeHandler.getVisiblePages(), stableZoom);
        }
        handleGonePages(/* clearViews= */ true);
    }

    /**  */
    public void refreshVisibleTiles(boolean requiresLayoutPass,
            @NonNull ViewState viewState) {
        if (requiresLayoutPass) {
            refreshTilesAfterLayout(viewState, mPageRangeHandler.getVisiblePages());
        } else {
            refreshTiles(mPageRangeHandler.getVisiblePages());
        }
    }

    private void clearPages(Range pages, boolean clearViews) {
        for (int page : pages) {
            // Don't cancel search - search results for the current search are always useful,
            // even for pages we can't see right now. Form filling operations should always
            // be executed against the document, even if the user has scrolled away from the page.
            mPdfLoader.cancelExceptSearchAndFormFilling(page);
            mPdfLoader.releasePage(page);
            if (clearViews) {
                removeViewAt(page);
            }
        }
    }

    private void loadPageRange(Range pages,
            float stableZoom) {
        for (int page : pages) {
            mPdfLoader.cancelAllTileBitmaps(page);
            PageMosaicView pageView = mPageViewFactory.getOrCreatePageView(
                    page,
                    PaginationUtils.getPageElevationInPixels(getContext()),
                    mModel.getPageSize(page));
            pageView.clearTiles();
            pageView.requestFastDrawAtZoom(stableZoom);
            pageView.refreshPageContentAndOverlays();
        }
    }

    private void refreshPages(Range pages, float stableZoom) {
        for (int page : pages) {
            PageMosaicView pageView = mPageViewFactory.getOrCreatePageView(
                    page,
                    PaginationUtils.getPageElevationInPixels(getContext()),
                    mModel.getPageSize(page));
            pageView.requestDrawAtZoom(stableZoom);
            pageView.refreshPageContentAndOverlays();
        }
    }

    private void refreshPagesAfterLayout(ViewState viewState, Range pages,
            float stableZoom) {
        ThreadUtils.postOnUiThread(
                () -> {
                    if (viewState != ViewState.NO_VIEW) {
                        refreshPages(pages, stableZoom);
                    }
                });
    }

    private void refreshTiles(Range pages) {
        for (int page : pages) {
            PageMosaicView pageView = mPageViewFactory.getOrCreatePageView(
                    page,
                    PaginationUtils.getPageElevationInPixels(getContext()),
                    mModel.getPageSize(page));
            pageView.requestTiles();
        }
    }

    private void refreshTilesAfterLayout(ViewState viewState, Range pages) {
        ThreadUtils.postOnUiThread(
                () -> {
                    if (viewState != ViewState.NO_VIEW) {
                        refreshTiles(pages);
                    }
                });
    }

    public void setConfigurationChanged(boolean configurationChanged) {
        this.mIsConfigurationChanged = configurationChanged;
    }

    public boolean isConfigurationChanged() {
        return mIsConfigurationChanged;
    }

    static class SavedState extends View.BaseSavedState {
        final PaginationModel mModel;

        SavedState(Parcel source) {
            super(source);
            mModel = ParcelCompat.readParcelable(source, null, PaginationModel.class);
        }

        SavedState(Parcel source, ClassLoader loader) {
            super(source);
            mModel = ParcelCompat.readParcelable(source, loader, PaginationModel.class);
        }

        SavedState(Parcelable superState, PaginationModel model) {
            super(superState);
            mModel = model;
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            super.writeToParcel(out, flags);
            out.writeParcelable(mModel, flags);
        }

        public static final ClassLoaderCreator<SavedState> CREATOR =
                new ClassLoaderCreator<SavedState>() {

                    @Override
                    public SavedState createFromParcel(Parcel in) {
                        return new SavedState(in);
                    }

                    @Override
                    public SavedState createFromParcel(Parcel source, ClassLoader loader) {
                        return new SavedState(source, loader);
                    }

                    @Override
                    public SavedState[] newArray(int size) {
                        return new SavedState[size];
                    }
                };
    }
}
