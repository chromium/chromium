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

package androidx.pdf.viewer.fragment

import android.content.ActivityNotFoundException
import android.content.ContentResolver
import android.content.Context
import android.content.res.Configuration
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import androidx.annotation.RequiresExtension
import androidx.annotation.RestrictTo
import androidx.core.os.BundleCompat
import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.pdf.R
import androidx.pdf.ViewState
import androidx.pdf.data.DisplayData
import androidx.pdf.data.FutureValue
import androidx.pdf.data.Openable
import androidx.pdf.fetcher.Fetcher
import androidx.pdf.find.FindInFileView
import androidx.pdf.metrics.EventCallback
import androidx.pdf.models.PageSelection
import androidx.pdf.select.SelectionActionMode
import androidx.pdf.util.AnnotationUtils
import androidx.pdf.util.ObservableValue.ValueObserver
import androidx.pdf.util.Observables
import androidx.pdf.util.Observables.ExposedValue
import androidx.pdf.util.Preconditions
import androidx.pdf.util.Uris
import androidx.pdf.viewer.ImmersiveModeRequester
import androidx.pdf.viewer.LayoutHandler
import androidx.pdf.viewer.LoadingView
import androidx.pdf.viewer.PageSelectionValueObserver
import androidx.pdf.viewer.PageViewFactory
import androidx.pdf.viewer.PaginatedView
import androidx.pdf.viewer.PaginationModel
import androidx.pdf.viewer.PdfSelectionHandles
import androidx.pdf.viewer.PdfSelectionModel
import androidx.pdf.viewer.SearchQueryObserver
import androidx.pdf.viewer.SelectedMatch
import androidx.pdf.viewer.SelectedMatchValueObserver
import androidx.pdf.viewer.SingleTapHandler
import androidx.pdf.viewer.ZoomScrollValueObserver
import androidx.pdf.viewer.fragment.insets.TranslateInsetsAnimationCallback
import androidx.pdf.viewer.loader.PdfLoader
import androidx.pdf.viewer.loader.PdfLoaderCallbacksImpl
import androidx.pdf.viewmodel.PdfLoaderViewModel
import androidx.pdf.widget.FastScrollView
import androidx.pdf.widget.ZoomView
import androidx.pdf.widget.ZoomView.ZoomScroll
import com.google.android.material.floatingactionbutton.FloatingActionButton
import java.io.IOException
import kotlinx.coroutines.launch

/**
 * A Fragment that renders a PDF document.
 *
 * <p>A [PdfViewerFragment] that can display paginated PDFs. The viewer includes a FAB for
 * annotation support and a search menu. Each page is rendered in its own View. Upon creation, this
 * fragment displays a loading spinner.
 *
 * <p>Rendering is done in 2 passes:
 * <ol>
 * <li>Layout: Request the page data, get the dimensions and set them as measure for the image view.
 * <li>Render: Create bitmap(s) at adequate dimensions and attach them to the page view.
 * </ol>
 *
 * <p>The layout pass is progressive: starts with a few first pages of the document, then reach
 * further as the user scrolls down (and ultimately spans the whole document). The rendering pass is
 * tightly limited to the currently visible pages. Pages that are scrolled past (become not visible)
 * have their bitmaps released to free up memory.
 *
 * <p>Note that every activity/fragment that uses this class has to be themed with Theme.AppCompat
 * or a theme that extends that theme.
 *
 * @see documentUri
 */
@RequiresExtension(extension = Build.VERSION_CODES.S, version = 13)
public open class PdfViewerFragment : Fragment() {

    // ViewModel to manage PdfLoader state
    private val viewModel: PdfLoaderViewModel by viewModels()

    /** Single access to the PDF document: loads contents asynchronously (bitmaps, text,...) */
    private var pdfLoader: PdfLoader? = null

    /** True when this Fragment's life-cycle is between [.onStart] and [.onStop]. */
    private var started = false

    /**
     * True when this Viewer is on-screen (but independent on whether it is actually started, so it
     * could be invisible, because obscured by another app). This value is controlled by [postEnter]
     * and [.exit].
     */
    private var onScreen = false

    /** Marks that [onEnter] must be run after [onCreateView]. */
    private var delayedEnter = false
    private var hasContents = false

    private var container: ViewGroup? = null
    private var viewState: ExposedValue<ViewState> =
        Observables.newExposedValueWithInitialValue(ViewState.NO_VIEW)
    private var zoomView: ZoomView? = null
    private var paginatedView: PaginatedView? = null
    private var fastScrollView: FastScrollView? = null
    private var selectionObserver: ValueObserver<PageSelection>? = null
    private var selectionActionMode: SelectionActionMode? = null
    private var localUri: Uri? = null

    private var fetcher: Fetcher? = null
    private var zoomScrollObserver: ValueObserver<ZoomScroll>? = null
    private var searchQueryObserver: ValueObserver<String>? = null
    private var selectedMatchObserver: ValueObserver<SelectedMatch>? = null
    private var pdfViewer: FrameLayout? = null
    private var findInFileView: FindInFileView? = null
    private var singleTapHandler: SingleTapHandler? = null

    /** Callbacks of PDF loading asynchronous tasks. */
    private var pdfLoaderCallbacks: PdfLoaderCallbacksImpl? = null

    /** A saved [.onContentsAvailable] runnable to be run after [.onCreateView]. */
    private var delayedContentsAvailable: Runnable? = null
    private var loadingView: LoadingView? = null
    private var paginationModel: PaginationModel? = null
    private var layoutHandler: LayoutHandler? = null
    private var pageViewFactory: PageViewFactory? = null
    private var selectionHandles: PdfSelectionHandles? = null
    private var annotationButton: FloatingActionButton? = null
    private var fileData: DisplayData? = null
    private var isFileRestoring: Boolean = false
    private var isAnnotationIntentResolvable = false
    private var documentLoaded = false

    /**
     * Specify whether [documentUri] is updated before fragment went in STARTED state.
     *
     * If true, we'll trigger a loadFile() operation as soon as fragment reaches STARTED state.
     */
    private var pendingDocumentLoad: Boolean = false

    private var mEventCallback: EventCallback? = null

    private val mImmersiveModeRequester: ImmersiveModeRequester =
        object : ImmersiveModeRequester {
            override fun requestImmersiveModeChange(enterImmersive: Boolean) {
                onRequestImmersiveMode(enterImmersive)
            }
        }

    /**
     * The URI of the PDF document to display defaulting to `null`.
     *
     * When this property is set, the fragment begins loading the PDF document. A visual indicator
     * is displayed while the document is being loaded. Once the loading is fully completed, the
     * [onLoadDocumentSuccess] callback is invoked. If an error occurs during the loading phase, the
     * [onLoadDocumentError] callback is invoked with the exception.
     *
     * <p>Note: This property is recommended to be set when the fragment is in the started state.
     */
    public var documentUri: Uri? = null
        set(value) {
            field = value

            // Check if the uri is different from the previous one or restoring the same one
            isFileRestoring =
                arguments?.let {
                    val savedUri = BundleCompat.getParcelable(it, KEY_DOCUMENT_URI, Uri::class.java)
                    savedUri?.equals(value) ?: false
                } ?: false

            // Load file if it's a new URI or it's not loaded
            if (value != null && (!isFileRestoring || !documentLoaded)) {
                loadFile(value)
            }
        }

    /**
     * Controls whether text search mode is active. Defaults to false.
     *
     * When text search mode is activated, the search menu becomes visible, and search functionality
     * is enabled. Deactivating text search mode hides the search menu, clears search results, and
     * removes any search-related highlights.
     *
     * <p>Note: This property should only be set once fragment is in the started state. Updating it
     * before will trigger an [IllegalStateException] which will be delivered through
     * [onLoadDocumentError] to host.
     */
    public var isTextSearchActive: Boolean = false
        set(value) {
            if (!isFileRestoring && !lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED)) {
                onLoadDocumentError(
                    IllegalStateException(
                        "Property can only be toggled after fragment's STARTED state"
                    )
                )
                return
            }
            field = value

            // Clear selection
            pdfLoaderCallbacks?.selectionModel?.setSelection(null)

            arguments?.putBoolean(KEY_TEXT_SEARCH_ACTIVE, value)
            findInFileView?.setFindInFileView(value)
        }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public fun setEventCallback(eventCallback: EventCallback) {
        this.mEventCallback = eventCallback
    }

    /**
     * Indicates whether the toolbox should be visible.
     *
     * The host app can control this property to show/hide the toolbox based on its state and the
     * `onRequestImmersiveMode` callback. The setter updates the UI elements within the fragment
     * accordingly.
     */
    public var isToolboxVisible: Boolean
        get() = arguments?.getBoolean(KEY_TOOLBOX_VISIBILITY) ?: true
        set(value) {
            (arguments ?: Bundle()).apply { putBoolean(KEY_TOOLBOX_VISIBILITY, value) }
            if (value) annotationButton?.show() else annotationButton?.hide()
        }

    /**
     * Called when the PDF view wants to enter or exit immersive mode based on user's interaction
     * with the content. Apps would typically hide their top bar or other navigational interface
     * when in immersive mode. The default implementation keeps toolbox visibility in sync with the
     * enterImmersive mode. It is recommended that apps keep this behaviour by calling
     * super.onRequestImmersiveMode while overriding this method.
     *
     * @param enterImmersive true to enter immersive mode, false to exit.
     */
    public open fun onRequestImmersiveMode(enterImmersive: Boolean) {
        // Update toolbox visibility
        isToolboxVisible = !enterImmersive
    }

    /**
     * Invoked when the document has been fully loaded, processed, and the initial pages are
     * displayed within the viewing area. This callback signifies that the document is ready for
     * user interaction.
     *
     * <p>Note that this callback is dispatched only when the fragment is fully created and not yet
     * destroyed, i.e., after [onCreate] has fully run and before [onDestroy] runs, and only on the
     * main thread.
     */
    public open fun onLoadDocumentSuccess() {}

    /**
     * Invoked when a problem arises during the loading process of the PDF document. This callback
     * provides details about the encountered error, allowing for appropriate error handling and
     * user notification.
     *
     * <p>Note that this callback is dispatched only when the fragment is fully created and not yet
     * destroyed, i.e., after [onCreate] has fully run and before [onDestroy] runs, and only on the
     * main thread.
     *
     * @param error [Throwable] that occurred during document loading.
     */
    @Suppress("UNUSED_PARAMETER") public open fun onLoadDocumentError(error: Throwable) {}

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        fetcher = Fetcher.build(requireContext(), 1)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        super.onCreateView(inflater, container, savedInstanceState)
        this.container = container

        pdfViewer = inflater.inflate(R.layout.pdf_viewer_container, container, false) as FrameLayout
        pdfViewer?.isScrollContainer = true
        fastScrollView = pdfViewer?.findViewById(R.id.fast_scroll_view)
        loadingView = pdfViewer?.findViewById(R.id.loadingView)
        paginatedView = fastScrollView?.findViewById(R.id.pdf_view)
        zoomView = pdfViewer?.findViewById(R.id.zoom_view)
        findInFileView = pdfViewer?.findViewById(R.id.search)
        findInFileView!!.setPaginatedView(paginatedView!!)
        findInFileView!!.setOnClosedButtonCallback { isTextSearchActive = false }
        annotationButton = pdfViewer?.findViewById(R.id.edit_fab)

        zoomView?.setMetricEventCallback(mEventCallback)
        findInFileView?.setOnVisibilityChangedListener { isVisible ->
            fastScrollView?.setScrubberVisibility(!isVisible)
        }

        // All views are inflated, update the view state.
        if (viewState.get() == ViewState.NO_VIEW || viewState.get() == ViewState.ERROR) {
            viewState.set(ViewState.VIEW_CREATED)
            // View Inflated, show loading view
            loadingView?.showLoadingView()
        }

        //  Restore documentLoaded state to determine if the document was successfully loaded
        //  before a potential configuration change or fragment replacement.
        documentLoaded = savedInstanceState?.getBoolean(KEY_DOCUMENT_LOADED) ?: false

        arguments?.let { args ->
            documentUri = BundleCompat.getParcelable(args, KEY_DOCUMENT_URI, Uri::class.java)
            isTextSearchActive = args.getBoolean(KEY_TEXT_SEARCH_ACTIVE)
            isToolboxVisible = args.getBoolean(KEY_TOOLBOX_VISIBILITY)
        }

        pdfLoaderCallbacks =
            PdfLoaderCallbacksImpl(
                context = requireContext(),
                fragmentManager = requireActivity().supportFragmentManager,
                fastScrollView = fastScrollView!!,
                zoomView = zoomView!!,
                paginatedView = paginatedView!!,
                findInFileView = findInFileView!!,
                isTextSearchActive = isTextSearchActive,
                viewState = viewState,
                onRequestPassword = { onScreen ->
                    if (!(isResumed && onScreen)) {
                        // This would happen if the service decides to start while we're in
                        // the background. The dialog code below would then crash. We can't just
                        // bypass it because then we'd have a started service with no loaded PDF
                        // and no means to load it. The best way is to just kill the service which
                        // will restart on the next onStart.
                        pdfLoader?.disconnect()
                        return@PdfLoaderCallbacksImpl true
                    }
                    return@PdfLoaderCallbacksImpl false
                },
                onDocumentLoaded = {
                    documentLoaded = true
                    onLoadDocumentSuccess()
                    annotationButton?.let {
                        if ((savedInstanceState == null) && isAnnotationIntentResolvable) {
                            onRequestImmersiveMode(false)
                        }
                    }

                    hideSpinner()
                    showPdfView()
                },
                onDocumentLoadFailure = { exception, showErrorView ->
                    // Update state to reflect document load failure.
                    documentLoaded = false
                    handleError(exception, showErrorView)
                },
                eventCallback = mEventCallback
            )

        setUpEditFab()
        if (savedInstanceState != null) {
            paginatedView?.isConfigurationChanged = true
        }

        if (!hasContents && delayedContentsAvailable == null) {
            if (savedInstanceState != null) {
                restoreContents(savedInstanceState)
            }
        }

        return pdfViewer
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Using lifecycleScope to collect the flow
        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.pdfLoaderStateFlow.collect { loader ->
                loader?.let {
                    pdfLoader = loader
                    setContents(savedInstanceState)
                }
            }
        }
        // Add listener to adjust bottom margin for [FindInFile] view
        findInFileView?.let {
            val windowManager = activity?.getSystemService(Context.WINDOW_SERVICE) as WindowManager

            ViewCompat.setWindowInsetsAnimationCallback(
                it,
                TranslateInsetsAnimationCallback(it, windowManager, container)
            )
        }

        loadPendingDocumentIfRequired()
    }

    private fun loadPendingDocumentIfRequired() {
        lifecycle.addObserver(
            object : DefaultLifecycleObserver {
                override fun onStart(owner: LifecycleOwner) {
                    super.onStart(owner)
                    // Check if we're pending on loading a document
                    if (pendingDocumentLoad) {
                        // Trigger load file
                        documentUri?.let { loadFile(it) }
                    }
                }
            }
        )
    }

    override fun onStart() {
        delayedContentsAvailable?.run()
        super.onStart()
        started = true

        // Check if the document file exists, return early if not
        documentUri?.let {
            val fileExist = checkAndFetchFile(it, false)
            if (!fileExist) {
                return
            }
        }
        if (delayedEnter || onScreen) {
            onEnter()
            delayedEnter = false
        }
    }

    override fun onStop() {
        if (onScreen) {
            onExit()
        }
        started = false
        super.onStop()
    }

    /** Called after this viewer enters the screen and becomes visible. */
    private fun onEnter() {
        participateInAccessibility(true)

        // This is necessary for password protected PDF documents. If the user failed to produce the
        // correct password, we want to prompt for the correct password every time the film strip
        // comes back to this viewer.
        if (!documentLoaded) {
            pdfLoader?.reconnect()
        }

        // Start Recording First Page Load Latency.
        mEventCallback?.onViewerVisible()

        if (paginatedView != null && paginatedView?.childCount!! > 0) {
            zoomView?.let { layoutHandler?.let { it1 -> it.loadPageAssets(it1, viewState) } }
        }
    }

    /** Called after this viewer exits the screen and becomes invisible to the user. */
    private fun onExit() {
        participateInAccessibility(false)
        if (!documentLoaded) {
            // e.g. a password-protected pdf that wasn't loaded.
            pdfLoader?.disconnect()
        }
    }

    /**
     * Notifies this Viewer goes on-screen. Guarantees that [.onEnter] will be called now or when
     * the Viewer is started.
     */
    private fun postEnter() {
        pdfLoaderCallbacks?.onScreen = true
        onScreen = true
        if (started) {
            onEnter()
        } else {
            delayedEnter = true
        }
    }

    private fun isStarted(): Boolean {
        return started
    }

    /**
     * Posts a [.onContentsAvailable] method to be run as soon as permitted (when this Viewer has
     * its view hierarchy built up and [onCreateView] has finished). It might run right now if the
     * Viewer is currently started.
     */
    private fun postContentsAvailable(contents: DisplayData) {
        Preconditions.checkState(delayedContentsAvailable == null, "Already waits for contents")

        if (isStarted()) {
            onContentsAvailable(contents)
            hasContents = true
        } else {
            delayedContentsAvailable = Runnable {
                Preconditions.checkState(
                    !hasContents,
                    "Received contents while restoring another copy"
                )
                onContentsAvailable(contents)
                delayedContentsAvailable = null
                hasContents = true
            }
        }
    }

    private fun onContentsAvailable(contents: DisplayData) {
        fileData = contents

        // Update the PdfLoader in the ViewModel with the new DisplayData
        viewModel.updatePdfLoader(
            requireActivity().applicationContext,
            contents,
            pdfLoaderCallbacks!!
        ) {
            zoomView?.setDocumentLoaded(/* documentLoaded= */ false)
        }
        setAnnotationIntentResolvability()
    }

    private fun setAnnotationIntentResolvability() {
        isAnnotationIntentResolvable =
            AnnotationUtils.resolveAnnotationIntent(requireContext(), documentUri!!)
        singleTapHandler?.setAnnotationIntentResolvable(isAnnotationIntentResolvable)
        findInFileView!!.setAnnotationIntentResolvable(isAnnotationIntentResolvable)
        (zoomScrollObserver as? ZoomScrollValueObserver)?.setAnnotationIntentResolvable(
            isAnnotationIntentResolvable
        )
    }

    /**
     * Sets PDF viewer content. Initializes/configures components based on provided data and saved
     * state.
     *
     * @param savedState Saved state (e.g., layout) or null.
     */
    private fun setContents(savedState: Bundle?) {
        savedState?.let { state ->
            if (isFileRestoring) {
                val showAnnotationButton = state.getBoolean(KEY_SHOW_ANNOTATION)
                isAnnotationIntentResolvable =
                    showAnnotationButton && findInFileView!!.visibility != View.VISIBLE
                if (
                    isAnnotationIntentResolvable &&
                    state.getBoolean(KEY_ANNOTATION_BUTTON_VISIBILITY)
                ) {
                    onRequestImmersiveMode(false)
                }
            }
        }

        refreshContentAndModels(pdfLoader!!)

        savedState?.let { state ->
            if (isFileRestoring) {
                state.containsKey(KEY_LAYOUT_REACH).let {
                    val layoutReach = state.getInt(KEY_LAYOUT_REACH, -1)
                    if (layoutReach != -1) {
                        layoutHandler?.pageLayoutReach = layoutReach
                        layoutHandler?.setInitialPageLayoutReachWithMax(layoutReach)
                    }
                }

                // Restore page selection from saved state if it exists
                val savedSelection =
                    BundleCompat.getParcelable(state, KEY_PAGE_SELECTION, PageSelection::class.java)
                savedSelection?.let { pdfLoaderCallbacks?.selectionModel?.setSelection(it) }
            }
        }
    }

    private fun updateSelectionModel(updatedSelectionModel: PdfSelectionModel) {
        pdfLoaderCallbacks?.selectionModel = updatedSelectionModel
        zoomView?.setPdfSelectionModel(updatedSelectionModel)
        paginatedView?.selectionModel = updatedSelectionModel

        selectionActionMode =
            SelectionActionMode(requireActivity(), paginatedView!!, updatedSelectionModel)
        selectionHandles =
            PdfSelectionHandles(
                updatedSelectionModel,
                zoomView!!,
                paginatedView!!,
                selectionActionMode!!
            )
        paginatedView?.selectionHandles = selectionHandles!!
    }

    private fun updatePageViewFactory(updatedPageViewFactory: PageViewFactory) {
        pageViewFactory = updatedPageViewFactory
        pdfLoaderCallbacks?.pageViewFactory = updatedPageViewFactory
        paginatedView?.pageViewFactory = updatedPageViewFactory
        paginatedView?.setMetricEventCallback(mEventCallback)

        selectionObserver =
            PageSelectionValueObserver(paginatedView!!, pageViewFactory!!, requireContext())
        pdfLoaderCallbacks?.selectionModel?.selection()?.addObserver(selectionObserver)
    }

    private fun updateSearchModel() {
        findInFileView?.searchModel?.let { model ->
            pdfLoaderCallbacks?.searchModel = model
            paginatedView?.searchModel = model
            searchQueryObserver = SearchQueryObserver(paginatedView!!)
            model.query().addObserver(searchQueryObserver)
        }
    }

    private fun updateSingleTapHandler(pdfLoader: PdfLoader, selectionModel: PdfSelectionModel) {
        singleTapHandler =
            SingleTapHandler(
                requireContext(),
                annotationButton!!,
                paginatedView!!,
                findInFileView!!,
                zoomView!!,
                selectionModel,
                paginationModel!!,
                layoutHandler!!,
                mImmersiveModeRequester
            )
        singleTapHandler!!.setAnnotationIntentResolvable(isAnnotationIntentResolvable)

        pageViewFactory =
            PageViewFactory(
                requireContext(),
                pdfLoader,
                paginatedView!!,
                zoomView!!,
                singleTapHandler!!,
                findInFileView!!,
                mEventCallback
            )
        updatePageViewFactory(pageViewFactory!!)
    }

    private fun refreshContentAndModels(pdfLoader: PdfLoader) {
        paginationModel = paginatedView!!.model

        paginatedView?.setPdfLoader(pdfLoader)
        findInFileView?.setPdfLoader(pdfLoader)
        pdfLoaderCallbacks?.pdfLoader = pdfLoader

        layoutHandler = LayoutHandler(pdfLoader)
        paginatedView?.model?.size?.let { layoutHandler!!.pageLayoutReach = it }

        val updatedSelectionModel = PdfSelectionModel(pdfLoader)
        updateSelectionModel(updatedSelectionModel)
        updateSingleTapHandler(pdfLoader, updatedSelectionModel)
        updateSearchModel()

        pdfLoaderCallbacks!!.layoutHandler = layoutHandler
        zoomScrollObserver =
            ZoomScrollValueObserver(
                zoomView!!,
                paginatedView!!,
                layoutHandler!!,
                annotationButton!!,
                findInFileView!!,
                isAnnotationIntentResolvable,
                selectionActionMode!!,
                viewState,
                mImmersiveModeRequester
            )
        zoomView?.zoomScroll()?.addObserver(zoomScrollObserver)

        selectedMatchObserver =
            SelectedMatchValueObserver(
                paginatedView!!,
                pageViewFactory!!,
                zoomView!!,
                layoutHandler!!,
                requireContext()
            )
        findInFileView!!.searchModel.selectedMatch().addObserver(selectedMatchObserver)

        annotationButton?.let { findInFileView!!.setAnnotationButton(it, mImmersiveModeRequester) }

        fastScrollView?.setOnFastScrollActiveListener {
            annotationButton?.let { button ->
                if (button.visibility == View.VISIBLE) {
                    onRequestImmersiveMode(true)
                }
            }
        }
    }

    /** Restores the contents of this Viewer when it is automatically restored by android. */
    private fun restoreContents(savedState: Bundle?) {
        pendingDocumentLoad = savedState?.getBoolean(KEY_PENDING_DOCUMENT_LOAD) ?: false
        val dataBundle = savedState?.getBundle(KEY_DATA)
        if (dataBundle != null) {
            try {
                fileData = DisplayData.fromBundle(dataBundle)
                fileData?.let {
                    localUri = it.uri
                    postContentsAvailable(it)
                    postEnter()
                }
            } catch (e: Exception) {
                // This can happen if the data is an instance of StreamOpenable, and the client
                // app that owns it has been killed by the system. We will still recover,
                // but log this.
                viewState.set(ViewState.ERROR)
                handleError(e)
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (!documentLoaded || paginatedView?.isConfigurationChanged == true) {
            return
        }
        setAnnotationIntentResolvability()
        if (!isAnnotationIntentResolvable && annotationButton?.visibility == View.VISIBLE) {
            annotationButton?.post { onRequestImmersiveMode(true) }
        }
        if (
            isAnnotationIntentResolvable &&
            annotationButton?.visibility != View.VISIBLE &&
            findInFileView?.visibility != View.VISIBLE
        ) {
            annotationButton?.post { onRequestImmersiveMode(false) }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        paginatedView?.isConfigurationChanged = true
    }

    private fun destroyContentModel() {
        pdfLoader?.cancelAll()
        paginationModel = null

        selectionHandles?.destroy()
        selectionHandles = null

        pdfLoaderCallbacks?.selectionModel = null
        selectionActionMode?.destroy()

        findInFileView?.searchModel?.let {
            it.selectedMatch().removeObserver(selectedMatchObserver!!)
            it.query().removeObserver(searchQueryObserver!!)
        }
        fastScrollView?.setOnFastScrollActiveListener(null)

        pdfLoaderCallbacks?.searchModel = null

        pdfLoader = null
        documentLoaded = false
    }

    private fun destroyView() {
        detachViewsAndObservers()
        zoomView = null
        paginatedView = null

        pdfLoader?.cancelAll()
        documentLoaded = false
        if (viewState.get() !== ViewState.NO_VIEW) {
            viewState.set(ViewState.NO_VIEW)
        }
        if (container != null && view != null && container === requireView().parent) {
            // Some viewers add extra views to their container, e.g. toasts. Remove them all.
            // Do not remove what's under it though.
            val count = container?.childCount
            var child: View
            if (count != null) {
                for (i in count - 1 downTo 1) {
                    child = container!!.getChildAt(i)
                    container?.removeView(child)
                    if (child === view) {
                        break
                    }
                }
            }
        }
    }

    private fun detachViewsAndObservers() {
        zoomScrollObserver?.let { zoomView?.zoomScroll()?.removeObserver(it) }
        paginatedView?.let { view -> view.removeAllViews() }
    }

    override fun onDestroyView() {
        destroyView()
        container = null
        pdfLoaderCallbacks = null
        super.onDestroyView()
        (zoomScrollObserver as? ZoomScrollValueObserver)?.clearAnnotationHandler()
    }

    override fun onDestroy() {
        super.onDestroy()
        if (pdfLoader != null) {
            destroyContentModel()
        }
        mEventCallback?.onViewerReset()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.apply {
            putBundle(KEY_DATA, fileData?.asBundle())
            layoutHandler?.let { putInt(KEY_LAYOUT_REACH, it.pageLayoutReach) }
            putBoolean(KEY_SHOW_ANNOTATION, isAnnotationIntentResolvable)
            pdfLoaderCallbacks?.selectionModel?.let {
                putParcelable(KEY_PAGE_SELECTION, it.selection().get())
            }
            putBoolean(
                KEY_ANNOTATION_BUTTON_VISIBILITY,
                (annotationButton?.visibility == View.VISIBLE)
            )
            putBoolean(KEY_PENDING_DOCUMENT_LOAD, pendingDocumentLoad)
            putBoolean(KEY_DOCUMENT_LOADED, documentLoaded)
        }
    }

    private fun handleError(error: Throwable, showErrorView: Boolean = true) {
        // Report exception occurred while processing PDF to host
        onLoadDocumentError(error)
        // Show a generic error message on pdf container, if required
        if (showErrorView)
            context?.resources?.getString(R.string.error_cannot_open_pdf)?.let {
                loadingView?.showErrorView(it)
            }
    }

    private fun resetViewsAndModels(fileUri: Uri) {
        if (pdfLoader != null) {
            pdfLoaderCallbacks?.uri = fileUri
            destroyContentModel()
        }
        paginatedView?.resetModels()
        fastScrollView?.resetContents()
        findInFileView?.resetFindInFile()
    }

    private fun loadFile(fileUri: Uri) {
        // Early return if fragment is not in STARTED state
        if (!lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED)) {
            // Update state to mark an early return
            pendingDocumentLoad = true
            return
        }
        // Update state as loadFile is triggered after in-or-after STARTED state
        pendingDocumentLoad = false

        arguments =
            Bundle().apply {
                putParcelable(KEY_DOCUMENT_URI, fileUri)
                putBoolean(KEY_TEXT_SEARCH_ACTIVE, false)
            }

        // Reset UI components and models before loading the file
        resetViewsAndModels(fileUri)
        detachViewsAndObservers()

        // Validate the file URI and attempt to load the file contents
        checkAndFetchFile(fileUri, true)

        if (localUri != null && localUri != fileUri) {
            onRequestImmersiveMode(true)
        }
        localUri = fileUri
    }

    private fun checkAndFetchFile(fileUri: Uri, performLoad: Boolean): Boolean {
        try {
            validateFileUri(fileUri)
            fetchFile(fileUri, performLoad)
            return true
        } catch (error: Exception) {
            when (error) {
                is IOException,
                is SecurityException,
                is NullPointerException -> handleFileNotAvailable(fileUri, error)
                else -> {
                    throw error
                }
            }
            return false
        }
    }

    private fun handleFileNotAvailable(fileUri: Uri, error: Throwable) {
        // Reset views and models when file error occurs
        resetViewsAndModels(fileUri)

        // Hide fast scroll and show loading view to display error message
        fastScrollView?.visibility = View.GONE
        loadingView?.visibility = View.VISIBLE

        handleError(error)
    }

    private fun validateFileUri(fileUri: Uri) {
        if (!Uris.isContentUri(fileUri) && !Uris.isFileUri(fileUri)) {
            throw IllegalArgumentException("Only content and file uri is supported")
        }
    }

    private fun fetchFile(fileUri: Uri, performLoad: Boolean) {
        Preconditions.checkNotNull(fileUri)
        val fileName: String = getFileName(fileUri)
        val openable: FutureValue<Openable> = fetcher?.loadLocal(fileUri)!!

        openable[
            object : FutureValue.Callback<Openable> {
                override fun available(value: Openable) {
                    // If loading is required, notify the viewer with the available content.
                    if (performLoad) {
                        viewerAvailable(fileUri, fileName, value)
                    }
                }

                override fun failed(thrown: Throwable) {
                    handleError(thrown)
                }

                override fun progress(progress: Float) {}
            }]
    }

    private fun getFileName(fileUri: Uri): String {
        val resolver: ContentResolver? = getResolver()
        return if (resolver != null) Uris.extractName(fileUri, resolver)
        else Uris.extractFileName(fileUri)
    }

    private fun getResolver(): ContentResolver? {
        if (activity != null) {
            return requireActivity().contentResolver
        }
        return null
    }

    private fun viewerAvailable(fileUri: Uri, fileName: String, openable: Openable) {
        val contents = DisplayData(fileUri, fileName, openable)

        startViewer(contents)
    }

    private fun startViewer(contents: DisplayData) {
        Preconditions.checkNotNull(contents)
        try {
            feed(contents)
            postEnter()
        } catch (exception: Exception) {
            onLoadDocumentError(exception)
        }
    }

    /** Feed this Viewer with contents to be displayed. */
    private fun feed(contents: DisplayData?): PdfViewerFragment {
        if (contents != null) {
            postContentsAvailable(contents)
        }
        return this
    }

    /** Makes the views of this Viewer visible to TalkBack (in the swipe gesture circus) or not. */
    private fun participateInAccessibility(participate: Boolean) {
        view?.importantForAccessibility =
            if (participate) View.IMPORTANT_FOR_ACCESSIBILITY_YES
            else View.IMPORTANT_FOR_ACCESSIBILITY_NO
    }

    private fun setUpEditFab() {
        annotationButton?.setOnClickListener(View.OnClickListener { performEdit() })
    }

    private fun performEdit() {
        setAnnotationIntentResolvability()
        // TODO: Fix the behavior of immersiveMode to be independent of isAnnotationIntentResolvable
        if (!isAnnotationIntentResolvable) {
            annotationButton?.hide()
            return
        }
        try {
            val intent = AnnotationUtils.getAnnotationIntent(localUri!!)
            intent.setData(localUri)
            intent.putExtra(EXTRA_PDF_FILE_NAME, getFileName(localUri!!))
            intent.putExtra(EXTRA_STARTING_PAGE, getStartingPageNumber())
            startActivity(intent)
        } catch (error: Exception) {
            when (error) {
                is ActivityNotFoundException,
                is NullPointerException -> hideAnnotationButtonAndShowToast()
                else -> throw error
            }
        }
    }

    private fun hideAnnotationButtonAndShowToast() {
        annotationButton?.hide()
        // TODO: Uncomment the Toast after translation for string is completed.
        /* Toast.makeText(
            context,
            context?.resources?.getString(R.string.cannot_edit_pdf),
            Toast.LENGTH_SHORT
        )
        .show() */
    }

    private fun getStartingPageNumber(): Int {
        // Return the page that is centered in the view.
        return paginationModel?.midPage ?: 0
    }

    private fun hideSpinner() {
        loadingView?.visibility = View.GONE
    }

    private fun showPdfView() {
        fastScrollView?.visibility = View.VISIBLE
    }

    private companion object {
        /** Key for saving page layout reach in bundles. */
        private const val KEY_LAYOUT_REACH: String = "plr"
        private const val KEY_DATA: String = "data"
        private const val KEY_TEXT_SEARCH_ACTIVE: String = "isTextSearchActive"
        private const val KEY_SHOW_ANNOTATION: String = "showEditFab"
        private const val KEY_PAGE_SELECTION: String = "currentPageSelection"
        private const val KEY_DOCUMENT_URI: String = "documentUri"
        private const val KEY_ANNOTATION_BUTTON_VISIBILITY = "isAnnotationVisible"
        private const val KEY_PENDING_DOCUMENT_LOAD = "pendingDocumentLoad"
        private const val KEY_TOOLBOX_VISIBILITY = "isToolboxVisible"
        private const val KEY_DOCUMENT_LOADED = "isDocumentLoaded"
        private const val EXTRA_PDF_FILE_NAME = "androidx.pdf.viewer.fragment.extra.PDF_FILE_NAME"
        private const val EXTRA_STARTING_PAGE: String =
            "androidx.pdf.viewer.fragment.extra.STARTING_PAGE"
    }
}
