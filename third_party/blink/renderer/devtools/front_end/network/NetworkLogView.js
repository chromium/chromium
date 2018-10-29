/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @implements {SDK.SDKModelObserver<!SDK.NetworkManager>}
 */
Network.NetworkLogView = class extends UI.VBox {
  /**
   * @param {!UI.FilterBar} filterBar
   * @param {!Element} progressBarContainer
   * @param {!Common.Setting} networkLogLargeRowsSetting
   */
  constructor(filterBar, progressBarContainer, networkLogLargeRowsSetting) {
    super();
    this.setMinimumSize(50, 64);
    this.registerRequiredCSS('network/networkLogView.css');

    this.element.id = 'network-container';

    this._networkHideDataURLSetting = Common.settings.createSetting('networkHideDataURL', false);
    this._networkResourceTypeFiltersSetting = Common.settings.createSetting('networkResourceTypeFilters', {});

    this._rawRowHeight = 0;
    this._progressBarContainer = progressBarContainer;
    this._networkLogLargeRowsSetting = networkLogLargeRowsSetting;
    this._networkLogLargeRowsSetting.addChangeListener(updateRowHeight.bind(this), this);

    /**
     * @this {Network.NetworkLogView}
     */
    function updateRowHeight() {
      this._rawRowHeight = !!this._networkLogLargeRowsSetting.get() ? 41 : 21;
      this._rowHeight = this._computeRowHeight();
    }
    this._rawRowHeight = 0;
    this._rowHeight = 0;
    updateRowHeight.call(this);

    /** @type {!Network.NetworkTransferTimeCalculator} */
    this._timeCalculator = new Network.NetworkTransferTimeCalculator();
    /** @type {!Network.NetworkTransferDurationCalculator} */
    this._durationCalculator = new Network.NetworkTransferDurationCalculator();
    this._calculator = this._timeCalculator;

    this._columns = new Network.NetworkLogViewColumns(
        this, this._timeCalculator, this._durationCalculator, networkLogLargeRowsSetting);
    this._columns.show(this.element);

    /** @type {!Set<!SDK.NetworkRequest>} */
    this._staleRequests = new Set();
    /** @type {number} */
    this._mainRequestLoadTime = -1;
    /** @type {number} */
    this._mainRequestDOMContentLoadedTime = -1;
    this._highlightedSubstringChanges = [];

    /** @type {!Array.<!Network.NetworkLogView.Filter>} */
    this._filters = [];
    /** @type {?Network.NetworkLogView.Filter} */
    this._timeFilter = null;
    /** @type {?Network.NetworkNode} */
    this._hoveredNode = null;
    /** @type {?Element} */
    this._recordingHint = null;
    /** @type {?number} */
    this._refreshRequestId = null;
    /** @type {?Network.NetworkRequestNode} */
    this._highlightedNode = null;

    this.linkifier = new Components.Linkifier();
    this.badgePool = new ProductRegistry.BadgePool();

    this._recording = false;
    this._needsRefresh = false;

    this._headerHeight = 0;

    /** @type {!Map<string, !Network.GroupLookupInterface>} */
    this._groupLookups = new Map();
    this._groupLookups.set('Frame', new Network.NetworkFrameGrouper(this));

    /** @type {?Network.GroupLookupInterface} */
    this._activeGroupLookup = null;

    this._textFilterUI = new UI.TextFilterUI();
    this._textFilterUI.addEventListener(UI.FilterUI.Events.FilterChanged, this._filterChanged, this);
    filterBar.addFilter(this._textFilterUI);

    this._dataURLFilterUI = new UI.CheckboxFilterUI(
        'hide-data-url', Common.UIString('Hide data URLs'), true, this._networkHideDataURLSetting);
    this._dataURLFilterUI.addEventListener(UI.FilterUI.Events.FilterChanged, this._filterChanged.bind(this), this);
    filterBar.addFilter(this._dataURLFilterUI);

    const filterItems =
        Object.values(Common.resourceCategories)
            .map(category => ({name: category.title, label: category.shortTitle, title: category.title}));
    this._resourceCategoryFilterUI = new UI.NamedBitSetFilterUI(filterItems, this._networkResourceTypeFiltersSetting);
    this._resourceCategoryFilterUI.addEventListener(
        UI.FilterUI.Events.FilterChanged, this._filterChanged.bind(this), this);
    filterBar.addFilter(this._resourceCategoryFilterUI);

    this._filterParser = new TextUtils.FilterParser(Network.NetworkLogView._searchKeys);
    this._suggestionBuilder =
        new UI.FilterSuggestionBuilder(Network.NetworkLogView._searchKeys, Network.NetworkLogView._sortSearchValues);
    this._resetSuggestionBuilder();

    this._dataGrid = this._columns.dataGrid();
    this._setupDataGrid();
    this._columns.sortByCurrentColumn();
    filterBar.filterButton().addEventListener(
        UI.ToolbarButton.Events.Click, this._dataGrid.scheduleUpdate.bind(this._dataGrid, true /* isFromUser */));

    this._summaryBarElement = this.element.createChild('div', 'network-summary-bar');

    new UI.DropTarget(
        this.element, [UI.DropTarget.Type.File], Common.UIString('Drop HAR files here'), this._handleDrop.bind(this));

    Common.moduleSetting('networkColorCodeResourceTypes')
        .addChangeListener(this._invalidateAllItems.bind(this, false), this);

    SDK.targetManager.observeModels(SDK.NetworkManager, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.RequestAdded, this._onRequestUpdated, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.RequestUpdated, this._onRequestUpdated, this);
    SDK.networkLog.addEventListener(SDK.NetworkLog.Events.Reset, this._reset, this);

    this._updateGroupByFrame();
    Common.moduleSetting('network.group-by-frame').addChangeListener(() => this._updateGroupByFrame());

    this._filterBar = filterBar;
  }

  _updateGroupByFrame() {
    const value = Common.moduleSetting('network.group-by-frame').get();
    this._setGrouping(value ? 'Frame' : null);
  }

  /**
   * @param {string} key
   * @param {!Array<string>} values
   */
  static _sortSearchValues(key, values) {
    if (key === Network.NetworkLogView.FilterType.Priority) {
      values.sort((a, b) => {
        const aPriority = /** @type {!Protocol.Network.ResourcePriority} */ (PerfUI.uiLabelToNetworkPriority(a));
        const bPriority = /** @type {!Protocol.Network.ResourcePriority} */ (PerfUI.uiLabelToNetworkPriority(b));
        return PerfUI.networkPriorityWeight(aPriority) - PerfUI.networkPriorityWeight(bPriority);
      });
    } else {
      values.sort();
    }
  }

  /**
   * @param {!Network.NetworkLogView.Filter} filter
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _negativeFilter(filter, request) {
    return !filter(request);
  }

  /**
   * @param {?RegExp} regex
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestPathFilter(regex, request) {
    if (!regex)
      return false;

    return regex.test(request.path() + '/' + request.name());
  }

  /**
   * @param {string} domain
   * @return {!Array.<string>}
   */
  static _subdomains(domain) {
    const result = [domain];
    let indexOfPeriod = domain.indexOf('.');
    while (indexOfPeriod !== -1) {
      result.push('*' + domain.substring(indexOfPeriod));
      indexOfPeriod = domain.indexOf('.', indexOfPeriod + 1);
    }
    return result;
  }

  /**
   * @param {string} value
   * @return {!Network.NetworkLogView.Filter}
   */
  static _createRequestDomainFilter(value) {
    /**
     * @param {string} string
     * @return {string}
     */
    function escapeForRegExp(string) {
      return string.escapeForRegExp();
    }
    const escapedPattern = value.split('*').map(escapeForRegExp).join('.*');
    return Network.NetworkLogView._requestDomainFilter.bind(null, new RegExp('^' + escapedPattern + '$', 'i'));
  }

  /**
   * @param {!RegExp} regex
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestDomainFilter(regex, request) {
    return regex.test(request.domain);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _runningRequestFilter(request) {
    return !request.finished;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _fromCacheRequestFilter(request) {
    return request.cached();
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestResponseHeaderFilter(value, request) {
    return request.responseHeaderValue(value) !== undefined;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestMethodFilter(value, request) {
    return request.requestMethod === value;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestPriorityFilter(value, request) {
    return request.priority() === value;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestMimeTypeFilter(value, request) {
    return request.mimeType === value;
  }

  /**
   * @param {!Network.NetworkLogView.MixedContentFilterValues} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestMixedContentFilter(value, request) {
    if (value === Network.NetworkLogView.MixedContentFilterValues.Displayed)
      return request.mixedContentType === Protocol.Security.MixedContentType.OptionallyBlockable;
    else if (value === Network.NetworkLogView.MixedContentFilterValues.Blocked)
      return request.mixedContentType === Protocol.Security.MixedContentType.Blockable && request.wasBlocked();
    else if (value === Network.NetworkLogView.MixedContentFilterValues.BlockOverridden)
      return request.mixedContentType === Protocol.Security.MixedContentType.Blockable && !request.wasBlocked();
    else if (value === Network.NetworkLogView.MixedContentFilterValues.All)
      return request.mixedContentType !== Protocol.Security.MixedContentType.None;

    return false;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestSchemeFilter(value, request) {
    return request.scheme === value;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestSetCookieDomainFilter(value, request) {
    const cookies = request.responseCookies;
    for (let i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
      if (cookies[i].domain() === value)
        return true;
    }
    return false;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestSetCookieNameFilter(value, request) {
    const cookies = request.responseCookies;
    for (let i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
      if (cookies[i].name() === value)
        return true;
    }
    return false;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestSetCookieValueFilter(value, request) {
    const cookies = request.responseCookies;
    for (let i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
      if (cookies[i].value() === value)
        return true;
    }
    return false;
  }

  /**
   * @param {number} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestSizeLargerThanFilter(value, request) {
    return request.transferSize >= value;
  }

  /**
   * @param {string} value
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _statusCodeFilter(value, request) {
    return ('' + request.statusCode) === value;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static HTTPRequestsFilter(request) {
    return request.parsedURL.isValid && (request.scheme in Network.NetworkLogView.HTTPSchemas);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static FinishedRequestsFilter(request) {
    return request.finished;
  }

  /**
   * @param {number} windowStart
   * @param {number} windowEnd
   * @param {!SDK.NetworkRequest} request
   * @return {boolean}
   */
  static _requestTimeFilter(windowStart, windowEnd, request) {
    if (request.issueTime() > windowEnd)
      return false;
    if (request.endTime !== -1 && request.endTime < windowStart)
      return false;
    return true;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  static _copyRequestHeaders(request) {
    InspectorFrontendHost.copyText(request.requestHeadersText());
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  static _copyResponseHeaders(request) {
    InspectorFrontendHost.copyText(request.responseHeadersText);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  static async _copyResponse(request) {
    const contentData = await request.contentData();
    let content = contentData.content || '';
    if (!request.contentType().isTextType())
      content = Common.ContentProvider.contentAsDataURL(content, request.mimeType, contentData.encoded);
    else if (contentData.encoded)
      content = window.atob(content);
    InspectorFrontendHost.copyText(content);
  }

  /**
   * @param {!DataTransfer} dataTransfer
   */
  _handleDrop(dataTransfer) {
    const items = dataTransfer.items;
    if (!items.length)
      return;
    const entry = items[0].webkitGetAsEntry();
    if (entry.isDirectory)
      return;

    entry.file(this._onLoadFromFile.bind(this));
  }

  /**
   * @param {!File} file
   */
  async _onLoadFromFile(file) {
    const outputStream = new Common.StringOutputStream();
    const reader = new Bindings.ChunkedFileReader(file, /* chunkSize */ 10000000);
    const success = await reader.read(outputStream);
    if (!success) {
      this._harLoadFailed(reader.error().message);
      return;
    }
    let harRoot;
    try {
      // HARRoot and JSON.parse might throw.
      harRoot = new HARImporter.HARRoot(JSON.parse(outputStream.data()));
    } catch (e) {
      this._harLoadFailed(e);
      return;
    }
    SDK.networkLog.importRequests(HARImporter.Importer.requestsFromHARLog(harRoot.log));
  }

  /**
   * @param {string} message
   */
  _harLoadFailed(message) {
    Common.console.error('Failed to load HAR file with following error: ' + message);
  }

  /**
   * @param {?string} groupKey
   */
  _setGrouping(groupKey) {
    if (this._activeGroupLookup)
      this._activeGroupLookup.reset();
    const groupLookup = groupKey ? this._groupLookups.get(groupKey) || null : null;
    this._activeGroupLookup = groupLookup;
    this._invalidateAllItems();
  }

  /**
   * @return {number}
   */
  _computeRowHeight() {
    return Math.round(this._rawRowHeight * window.devicePixelRatio) / window.devicePixelRatio;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {?Network.NetworkRequestNode}
   */
  nodeForRequest(request) {
    return request[Network.NetworkLogView._networkNodeSymbol] || null;
  }

  /**
   * @return {number}
   */
  headerHeight() {
    return this._headerHeight;
  }

  /**
   * @param {boolean} recording
   */
  setRecording(recording) {
    this._recording = recording;
    this._updateSummaryBar();
  }

  /**
   * @override
   * @param {!SDK.NetworkManager} networkManager
   */
  modelAdded(networkManager) {
    // TODO(allada) Remove dependency on networkManager and instead use NetworkLog and PageLoad for needed data.
    if (networkManager.target().parentTarget())
      return;
    const resourceTreeModel = networkManager.target().model(SDK.ResourceTreeModel);
    if (resourceTreeModel) {
      resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, this._loadEventFired, this);
      resourceTreeModel.addEventListener(
          SDK.ResourceTreeModel.Events.DOMContentLoaded, this._domContentLoadedEventFired, this);
    }
  }

  /**
   * @override
   * @param {!SDK.NetworkManager} networkManager
   */
  modelRemoved(networkManager) {
    if (!networkManager.target().parentTarget()) {
      const resourceTreeModel = networkManager.target().model(SDK.ResourceTreeModel);
      if (resourceTreeModel) {
        resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.Load, this._loadEventFired, this);
        resourceTreeModel.removeEventListener(
            SDK.ResourceTreeModel.Events.DOMContentLoaded, this._domContentLoadedEventFired, this);
      }
    }
  }

  /**
   * @param {number} start
   * @param {number} end
   */
  setWindow(start, end) {
    if (!start && !end) {
      this._timeFilter = null;
      this._timeCalculator.setWindow(null);
    } else {
      this._timeFilter = Network.NetworkLogView._requestTimeFilter.bind(null, start, end);
      this._timeCalculator.setWindow(new Network.NetworkTimeBoundary(start, end));
    }
    this._filterRequests();
  }

  clearSelection() {
    if (this._dataGrid.selectedNode)
      this._dataGrid.selectedNode.deselect();
  }

  _resetSuggestionBuilder() {
    this._suggestionBuilder.clear();
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.Is, Network.NetworkLogView.IsFilterType.Running);
    this._suggestionBuilder.addItem(
        Network.NetworkLogView.FilterType.Is, Network.NetworkLogView.IsFilterType.FromCache);
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.LargerThan, '100');
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.LargerThan, '10k');
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.LargerThan, '1M');
    this._textFilterUI.setSuggestionProvider(this._suggestionBuilder.completions.bind(this._suggestionBuilder));
  }

  /**
   * @param {!Common.Event} event
   */
  _filterChanged(event) {
    this.removeAllNodeHighlights();
    this._parseFilterQuery(this._textFilterUI.value());
    this._filterRequests();
  }

  _showRecordingHint() {
    this._hideRecordingHint();
    this._recordingHint = this.element.createChild('div', 'network-status-pane fill');
    const hintText = this._recordingHint.createChild('div', 'recording-hint');
    const reloadShortcutNode = this._recordingHint.createChild('b');
    reloadShortcutNode.textContent = UI.shortcutRegistry.shortcutDescriptorsForAction('inspector_main.reload')[0].name;

    if (this._recording) {
      const recordingText = hintText.createChild('span');
      recordingText.textContent = Common.UIString('Recording network activity\u2026');
      hintText.createChild('br');
      hintText.appendChild(
          UI.formatLocalized('Perform a request or hit %s to record the reload.', [reloadShortcutNode]));
    } else {
      const recordNode = hintText.createChild('b');
      recordNode.textContent = UI.shortcutRegistry.shortcutTitleForAction('network.toggle-recording');
      hintText.appendChild(UI.formatLocalized(
          'Record (%s) or reload (%s) to display network activity.', [recordNode, reloadShortcutNode]));
    }
  }

  _hideRecordingHint() {
    if (this._recordingHint)
      this._recordingHint.remove();
    this._recordingHint = null;
  }

  /**
   * @override
   * @return {!Array.<!Element>}
   */
  elementsToRestoreScrollPositionsFor() {
    if (!this._dataGrid)  // Not initialized yet.
      return [];
    return [this._dataGrid.scrollContainer];
  }

  columnExtensionResolved() {
    this._invalidateAllItems(true);
  }

  _setupDataGrid() {
    this._dataGrid.setRowContextMenuCallback((contextMenu, node) => {
      const request = node.request();
      if (request)
        this.handleContextMenuForRequest(contextMenu, request);
    });
    this._dataGrid.setStickToBottom(true);
    this._dataGrid.setName('networkLog');
    this._dataGrid.setResizeMethod(DataGrid.DataGrid.ResizeMethod.Last);
    this._dataGrid.element.classList.add('network-log-grid');
    this._dataGrid.element.addEventListener('mousedown', this._dataGridMouseDown.bind(this), true);
    this._dataGrid.element.addEventListener('mousemove', this._dataGridMouseMove.bind(this), true);
    this._dataGrid.element.addEventListener('mouseleave', () => this._setHoveredNode(null), true);
    return this._dataGrid;
  }

  /**
   * @param {!Event} event
   */
  _dataGridMouseMove(event) {
    const node = (this._dataGrid.dataGridNodeFromNode(/** @type {!Node} */ (event.target)));
    const highlightInitiatorChain = event.shiftKey;
    this._setHoveredNode(node, highlightInitiatorChain);
  }

  /**
   * @return {?Network.NetworkNode}
   */
  hoveredNode() {
    return this._hoveredNode;
  }

  /**
   * @param {?Network.NetworkNode} node
   * @param {boolean=} highlightInitiatorChain
   */
  _setHoveredNode(node, highlightInitiatorChain) {
    if (this._hoveredNode)
      this._hoveredNode.setHovered(false, false);
    this._hoveredNode = node;
    if (this._hoveredNode)
      this._hoveredNode.setHovered(true, !!highlightInitiatorChain);
  }

  /**
   * @param {!Event} event
   */
  _dataGridMouseDown(event) {
    if (!this._dataGrid.selectedNode && event.button)
      event.consume();
  }

  _updateSummaryBar() {
    this._hideRecordingHint();

    let transferSize = 0;
    let selectedNodeNumber = 0;
    let selectedTransferSize = 0;
    let baseTime = -1;
    let maxTime = -1;

    let nodeCount = 0;
    for (const request of SDK.networkLog.requests()) {
      const node = request[Network.NetworkLogView._networkNodeSymbol];
      if (!node)
        continue;
      nodeCount++;
      const requestTransferSize = request.transferSize;
      transferSize += requestTransferSize;
      if (!node[Network.NetworkLogView._isFilteredOutSymbol]) {
        selectedNodeNumber++;
        selectedTransferSize += requestTransferSize;
      }
      const networkManager = SDK.NetworkManager.forRequest(request);
      // TODO(allada) inspectedURL should be stored in PageLoad used instead of target so HAR requests can have an
      // inspected url.
      if (networkManager && request.url() === networkManager.target().inspectedURL() &&
          request.resourceType() === Common.resourceTypes.Document && !networkManager.target().parentTarget())
        baseTime = request.startTime;
      if (request.endTime > maxTime)
        maxTime = request.endTime;
    }

    if (!nodeCount) {
      this._showRecordingHint();
      return;
    }

    const summaryBar = this._summaryBarElement;
    summaryBar.removeChildren();
    const separator = '\u2002\u2758\u2002';
    let text = '';
    /**
     * @param {string} chunk
     * @return {!Element}
     */
    function appendChunk(chunk) {
      const span = summaryBar.createChild('span');
      span.textContent = chunk;
      text += chunk;
      return span;
    }

    if (selectedNodeNumber !== nodeCount) {
      appendChunk(Common.UIString('%d / %d requests', selectedNodeNumber, nodeCount));
      appendChunk(separator);
      appendChunk(Common.UIString(
          '%s / %s transferred', Number.bytesToString(selectedTransferSize), Number.bytesToString(transferSize)));
    } else {
      appendChunk(Common.UIString('%d requests', nodeCount));
      appendChunk(separator);
      appendChunk(Common.UIString('%s transferred', Number.bytesToString(transferSize)));
    }
    if (baseTime !== -1 && maxTime !== -1) {
      appendChunk(separator);
      appendChunk(Common.UIString('Finish: %s', Number.secondsToString(maxTime - baseTime)));
      if (this._mainRequestDOMContentLoadedTime !== -1 && this._mainRequestDOMContentLoadedTime > baseTime) {
        appendChunk(separator);
        const domContentLoadedText = Common.UIString(
            'DOMContentLoaded: %s', Number.secondsToString(this._mainRequestDOMContentLoadedTime - baseTime));
        appendChunk(domContentLoadedText).classList.add('summary-blue');
      }
      if (this._mainRequestLoadTime !== -1) {
        appendChunk(separator);
        const loadText = Common.UIString('Load: %s', Number.secondsToString(this._mainRequestLoadTime - baseTime));
        appendChunk(loadText).classList.add('summary-red');
      }
    }
    summaryBar.title = text;
  }

  scheduleRefresh() {
    if (this._needsRefresh)
      return;

    this._needsRefresh = true;

    if (this.isShowing() && !this._refreshRequestId)
      this._refreshRequestId = this.element.window().requestAnimationFrame(this._refresh.bind(this));
  }

  /**
   * @param {!Array<number>} times
   */
  addFilmStripFrames(times) {
    this._columns.addEventDividers(times, 'network-frame-divider');
  }

  /**
   * @param {number} time
   */
  selectFilmStripFrame(time) {
    this._columns.selectFilmStripFrame(time);
  }

  clearFilmStripFrame() {
    this._columns.clearFilmStripFrame();
  }

  _refreshIfNeeded() {
    if (this._needsRefresh)
      this._refresh();
  }

  /**
   * @param {boolean=} deferUpdate
   */
  _invalidateAllItems(deferUpdate) {
    this._staleRequests = new Set(SDK.networkLog.requests());
    if (deferUpdate)
      this.scheduleRefresh();
    else
      this._refresh();
  }

  /**
   * @return {!Network.NetworkTimeCalculator}
   */
  timeCalculator() {
    return this._timeCalculator;
  }

  /**
   * @return {!Network.NetworkTimeCalculator}
   */
  calculator() {
    return this._calculator;
  }

  /**
   * @param {!Network.NetworkTimeCalculator} x
   */
  setCalculator(x) {
    if (!x || this._calculator === x)
      return;

    if (this._calculator !== x) {
      this._calculator = x;
      this._columns.setCalculator(this._calculator);
    }
    this._calculator.reset();

    if (this._calculator.startAtZero)
      this._columns.hideEventDividers();
    else
      this._columns.showEventDividers();

    this._invalidateAllItems();
  }

  /**
   * @param {!Common.Event} event
   */
  _loadEventFired(event) {
    if (!this._recording)
      return;

    const time = /** @type {number} */ (event.data.loadTime);
    if (time) {
      this._mainRequestLoadTime = time;
      this._columns.addEventDividers([time], 'network-red-divider');
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _domContentLoadedEventFired(event) {
    if (!this._recording)
      return;
    const data = /** @type {number} */ (event.data);
    if (data) {
      this._mainRequestDOMContentLoadedTime = data;
      this._columns.addEventDividers([data], 'network-blue-divider');
    }
  }

  /**
   * @override
   */
  wasShown() {
    this._refreshIfNeeded();
    this._columns.wasShown();
  }

  /**
   * @override
   */
  willHide() {
    this._columns.willHide();
  }

  /**
   * @override
   */
  onResize() {
    this._rowHeight = this._computeRowHeight();
  }

  /**
   * @return {!Array<!Network.NetworkNode>}
   */
  flatNodesList() {
    return this._dataGrid.rootNode().flatChildren();
  }

  stylesChanged() {
    this._columns.scheduleRefresh();
  }

  _refresh() {
    this._needsRefresh = false;

    if (this._refreshRequestId) {
      this.element.window().cancelAnimationFrame(this._refreshRequestId);
      this._refreshRequestId = null;
    }

    this.removeAllNodeHighlights();

    this._timeCalculator.updateBoundariesForEventTime(this._mainRequestLoadTime);
    this._durationCalculator.updateBoundariesForEventTime(this._mainRequestLoadTime);
    this._timeCalculator.updateBoundariesForEventTime(this._mainRequestDOMContentLoadedTime);
    this._durationCalculator.updateBoundariesForEventTime(this._mainRequestDOMContentLoadedTime);

    /** @type {!Map<!Network.NetworkNode, !Network.NetworkNode>} */
    const nodesToInsert = new Map();
    /** @type {!Array<!Network.NetworkNode>} */
    const nodesToRefresh = [];

    /** @type {!Set<!Network.NetworkRequestNode>} */
    const staleNodes = new Set();

    // While creating nodes it may add more entries into _staleRequests because redirect request nodes update the parent
    // node so we loop until we have no more stale requests.
    while (this._staleRequests.size) {
      const request = this._staleRequests.firstValue();
      this._staleRequests.delete(request);
      let node = request[Network.NetworkLogView._networkNodeSymbol];
      if (!node)
        node = this._createNodeForRequest(request);
      staleNodes.add(node);
    }

    for (const node of staleNodes) {
      const isFilteredOut = !this._applyFilter(node);
      if (isFilteredOut && node === this._hoveredNode)
        this._setHoveredNode(null);

      if (!isFilteredOut)
        nodesToRefresh.push(node);
      const request = node.request();
      this._timeCalculator.updateBoundaries(request);
      this._durationCalculator.updateBoundaries(request);
      const newParent = this._parentNodeForInsert(node);
      if (node[Network.NetworkLogView._isFilteredOutSymbol] === isFilteredOut && node.parent === newParent)
        continue;
      node[Network.NetworkLogView._isFilteredOutSymbol] = isFilteredOut;
      const removeFromParent = node.parent && (isFilteredOut || node.parent !== newParent);
      if (removeFromParent) {
        let parent = node.parent;
        parent.removeChild(node);
        while (parent && !parent.hasChildren() && parent.dataGrid && parent.dataGrid.rootNode() !== parent) {
          const grandparent = parent.parent;
          grandparent.removeChild(parent);
          parent = grandparent;
        }
      }

      if (!newParent || isFilteredOut)
        continue;

      if (!newParent.dataGrid && !nodesToInsert.has(newParent)) {
        nodesToInsert.set(newParent, this._dataGrid.rootNode());
        nodesToRefresh.push(newParent);
      }
      nodesToInsert.set(node, newParent);
    }

    for (const node of nodesToInsert.keys())
      nodesToInsert.get(node).appendChild(node);

    for (const node of nodesToRefresh)
      node.refresh();

    this._updateSummaryBar();

    if (nodesToInsert.size)
      this._columns.sortByCurrentColumn();

    this._dataGrid.updateInstantly();
    this._didRefreshForTest();
  }

  _didRefreshForTest() {
  }

  /**
   * @param {!Network.NetworkRequestNode} node
   * @return {?Network.NetworkNode}
   */
  _parentNodeForInsert(node) {
    if (!this._activeGroupLookup)
      return this._dataGrid.rootNode();

    const groupNode = this._activeGroupLookup.groupNodeForRequest(node.request());
    if (!groupNode)
      return this._dataGrid.rootNode();
    return groupNode;
  }

  _reset() {
    this.dispatchEventToListeners(Network.NetworkLogView.Events.RequestSelected, null);

    this._setHoveredNode(null);
    this._columns.reset();

    this._timeFilter = null;
    this._calculator.reset();

    this._timeCalculator.setWindow(null);
    this.linkifier.reset();
    this.badgePool.reset();

    if (this._activeGroupLookup)
      this._activeGroupLookup.reset();
    this._staleRequests.clear();
    this._resetSuggestionBuilder();

    this._mainRequestLoadTime = -1;
    this._mainRequestDOMContentLoadedTime = -1;

    this._dataGrid.rootNode().removeChildren();
    this._updateSummaryBar();
    this._dataGrid.setStickToBottom(true);
    this.scheduleRefresh();
  }

  /**
   * @param {string} filterString
   */
  setTextFilterValue(filterString) {
    this._textFilterUI.setValue(filterString);
    this._dataURLFilterUI.setChecked(false);
    this._resourceCategoryFilterUI.reset();
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  _createNodeForRequest(request) {
    const node = new Network.NetworkRequestNode(this, request);
    request[Network.NetworkLogView._networkNodeSymbol] = node;
    node[Network.NetworkLogView._isFilteredOutSymbol] = true;

    for (let redirect = request.redirectSource(); redirect; redirect = redirect.redirectSource())
      this._refreshRequest(redirect);
    return node;
  }

  /**
   * @param {!Common.Event} event
   */
  _onRequestUpdated(event) {
    const request = /** @type {!SDK.NetworkRequest} */ (event.data);
    this._refreshRequest(request);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  _refreshRequest(request) {
    Network.NetworkLogView._subdomains(request.domain)
        .forEach(
            this._suggestionBuilder.addItem.bind(this._suggestionBuilder, Network.NetworkLogView.FilterType.Domain));
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.Method, request.requestMethod);
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.MimeType, request.mimeType);
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.Scheme, '' + request.scheme);
    this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.StatusCode, '' + request.statusCode);

    const priority = request.priority();
    if (priority) {
      this._suggestionBuilder.addItem(
          Network.NetworkLogView.FilterType.Priority, PerfUI.uiLabelForNetworkPriority(priority));
    }

    if (request.mixedContentType !== Protocol.Security.MixedContentType.None) {
      this._suggestionBuilder.addItem(
          Network.NetworkLogView.FilterType.MixedContent, Network.NetworkLogView.MixedContentFilterValues.All);
    }

    if (request.mixedContentType === Protocol.Security.MixedContentType.OptionallyBlockable) {
      this._suggestionBuilder.addItem(
          Network.NetworkLogView.FilterType.MixedContent, Network.NetworkLogView.MixedContentFilterValues.Displayed);
    }

    if (request.mixedContentType === Protocol.Security.MixedContentType.Blockable) {
      const suggestion = request.wasBlocked() ? Network.NetworkLogView.MixedContentFilterValues.Blocked :
                                                Network.NetworkLogView.MixedContentFilterValues.BlockOverridden;
      this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.MixedContent, suggestion);
    }

    const responseHeaders = request.responseHeaders;
    for (let i = 0, l = responseHeaders.length; i < l; ++i)
      this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.HasResponseHeader, responseHeaders[i].name);
    const cookies = request.responseCookies;
    for (let i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
      const cookie = cookies[i];
      this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.SetCookieDomain, cookie.domain());
      this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.SetCookieName, cookie.name());
      this._suggestionBuilder.addItem(Network.NetworkLogView.FilterType.SetCookieValue, cookie.value());
    }

    this._staleRequests.add(request);
    this.scheduleRefresh();
  }

  /**
   * @return {number}
   */
  rowHeight() {
    return this._rowHeight;
  }

  /**
   * @param {boolean} gridMode
   */
  switchViewMode(gridMode) {
    this._columns.switchViewMode(gridMode);
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   * @param {!SDK.NetworkRequest} request
   */
  handleContextMenuForRequest(contextMenu, request) {
    contextMenu.appendApplicableItems(request);
    let copyMenu = contextMenu.clipboardSection().appendSubMenuItem(Common.UIString('Copy'));
    const footerSection = copyMenu.footerSection();
    if (request) {
      copyMenu.defaultSection().appendItem(
          UI.copyLinkAddressLabel(), InspectorFrontendHost.copyText.bind(InspectorFrontendHost, request.contentURL()));
      if (request.requestHeadersText()) {
        copyMenu.defaultSection().appendItem(
            Common.UIString('Copy request headers'), Network.NetworkLogView._copyRequestHeaders.bind(null, request));
      }

      if (request.responseHeadersText) {
        copyMenu.defaultSection().appendItem(
            Common.UIString('Copy response headers'), Network.NetworkLogView._copyResponseHeaders.bind(null, request));
      }

      if (request.finished) {
        copyMenu.defaultSection().appendItem(
            Common.UIString('Copy response'), Network.NetworkLogView._copyResponse.bind(null, request));
      }

      const disableIfBlob = request.isBlobRequest();
      if (Host.isWin()) {
        footerSection.appendItem(
            Common.UIString('Copy as PowerShell'), this._copyPowerShellCommand.bind(this, request), disableIfBlob);
        footerSection.appendItem(
            Common.UIString('Copy as fetch'), this._copyFetchCall.bind(this, request), disableIfBlob);
        footerSection.appendItem(
            Common.UIString('Copy as cURL (cmd)'), this._copyCurlCommand.bind(this, request, 'win'), disableIfBlob);
        footerSection.appendItem(
            Common.UIString('Copy as cURL (bash)'), this._copyCurlCommand.bind(this, request, 'unix'), disableIfBlob);
        footerSection.appendItem(Common.UIString('Copy all as PowerShell'), this._copyAllPowerShellCommand.bind(this));
        footerSection.appendItem(Common.UIString('Copy all as fetch'), this._copyAllFetchCall.bind(this));
        footerSection.appendItem(Common.UIString('Copy all as cURL (cmd)'), this._copyAllCurlCommand.bind(this, 'win'));
        footerSection.appendItem(
            Common.UIString('Copy all as cURL (bash)'), this._copyAllCurlCommand.bind(this, 'unix'));
      } else {
        footerSection.appendItem(
            Common.UIString('Copy as fetch'), this._copyFetchCall.bind(this, request), disableIfBlob);
        footerSection.appendItem(
            Common.UIString('Copy as cURL'), this._copyCurlCommand.bind(this, request, 'unix'), disableIfBlob);
        footerSection.appendItem(Common.UIString('Copy all as fetch'), this._copyAllFetchCall.bind(this));
        footerSection.appendItem(Common.UIString('Copy all as cURL'), this._copyAllCurlCommand.bind(this, 'unix'));
      }
    } else {
      copyMenu = contextMenu.clipboardSection().appendSubMenuItem(Common.UIString('Copy'));
    }
    footerSection.appendItem(Common.UIString('Copy all as HAR'), this._copyAll.bind(this));

    contextMenu.saveSection().appendItem(Common.UIString('Save as HAR with content'), this._exportAll.bind(this));

    contextMenu.editSection().appendItem(Common.UIString('Clear browser cache'), this._clearBrowserCache.bind(this));
    contextMenu.editSection().appendItem(
        Common.UIString('Clear browser cookies'), this._clearBrowserCookies.bind(this));

    if (request) {
      const maxBlockedURLLength = 20;
      const manager = SDK.multitargetNetworkManager;
      let patterns = manager.blockedPatterns();

      const urlWithoutScheme = request.parsedURL.urlWithoutScheme();
      if (urlWithoutScheme && !patterns.find(pattern => pattern.url === urlWithoutScheme)) {
        contextMenu.debugSection().appendItem(
            Common.UIString('Block request URL'), addBlockedURL.bind(null, urlWithoutScheme));
      } else if (urlWithoutScheme) {
        const croppedURL = urlWithoutScheme.trimMiddle(maxBlockedURLLength);
        contextMenu.debugSection().appendItem(
            Common.UIString('Unblock %s', croppedURL), removeBlockedURL.bind(null, urlWithoutScheme));
      }

      const domain = request.parsedURL.domain();
      if (domain && !patterns.find(pattern => pattern.url === domain)) {
        contextMenu.debugSection().appendItem(
            Common.UIString('Block request domain'), addBlockedURL.bind(null, domain));
      } else if (domain) {
        const croppedDomain = domain.trimMiddle(maxBlockedURLLength);
        contextMenu.debugSection().appendItem(
            Common.UIString('Unblock %s', croppedDomain), removeBlockedURL.bind(null, domain));
      }

      if (SDK.NetworkManager.canReplayRequest(request)) {
        contextMenu.debugSection().appendItem(
            Common.UIString('Replay XHR'), SDK.NetworkManager.replayRequest.bind(null, request));
      }

      /**
       * @param {string} url
       */
      function addBlockedURL(url) {
        patterns.push({enabled: true, url: url});
        manager.setBlockedPatterns(patterns);
        manager.setBlockingEnabled(true);
        UI.viewManager.showView('network.blocked-urls');
      }

      /**
       * @param {string} url
       */
      function removeBlockedURL(url) {
        patterns = patterns.filter(pattern => pattern.url !== url);
        manager.setBlockedPatterns(patterns);
        UI.viewManager.showView('network.blocked-urls');
      }
    }
  }

  _harRequests() {
    const httpRequests = SDK.networkLog.requests().filter(Network.NetworkLogView.HTTPRequestsFilter);
    return httpRequests.filter(Network.NetworkLogView.FinishedRequestsFilter);
  }

  async _copyAll() {
    const harArchive = {log: await SDK.HARLog.build(this._harRequests())};
    InspectorFrontendHost.copyText(JSON.stringify(harArchive, null, 2));
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {string} platform
   */
  async _copyCurlCommand(request, platform) {
    const command = await this._generateCurlCommand(request, platform);
    InspectorFrontendHost.copyText(command);
  }

  /**
   * @param {string} platform
   */
  async _copyAllCurlCommand(platform) {
    const commands = await this._generateAllCurlCommand(SDK.networkLog.requests(), platform);
    InspectorFrontendHost.copyText(commands);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {string} platform
   */
  async _copyFetchCall(request, platform) {
    const command = await this._generateFetchCall(request);
    InspectorFrontendHost.copyText(command);
  }

  async _copyAllFetchCall() {
    const commands = await this._generateAllFetchCall(SDK.networkLog.requests());
    InspectorFrontendHost.copyText(commands);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  async _copyPowerShellCommand(request) {
    const command = await this._generatePowerShellCommand(request);
    InspectorFrontendHost.copyText(command);
  }

  async _copyAllPowerShellCommand() {
    const commands = this._generateAllPowerShellCommand(SDK.networkLog.requests());
    InspectorFrontendHost.copyText(commands);
  }

  async _exportAll() {
    const url = SDK.targetManager.mainTarget().inspectedURL();
    const parsedURL = url.asParsedURL();
    const filename = parsedURL ? parsedURL.host : 'network-log';
    const stream = new Bindings.FileOutputStream();

    if (!await stream.open(filename + '.har'))
      return;

    const progressIndicator = new UI.ProgressIndicator();
    this._progressBarContainer.appendChild(progressIndicator.element);
    await Network.HARWriter.write(stream, this._harRequests(), progressIndicator);
    progressIndicator.done();
    stream.close();
  }

  _clearBrowserCache() {
    if (confirm(Common.UIString('Are you sure you want to clear browser cache?')))
      SDK.multitargetNetworkManager.clearBrowserCache();
  }

  _clearBrowserCookies() {
    if (confirm(Common.UIString('Are you sure you want to clear browser cookies?')))
      SDK.multitargetNetworkManager.clearBrowserCookies();
  }

  _removeAllHighlights() {
    this.removeAllNodeHighlights();
    for (let i = 0; i < this._highlightedSubstringChanges.length; ++i)
      UI.revertDomChanges(this._highlightedSubstringChanges[i]);
    this._highlightedSubstringChanges = [];
  }

  /**
   * @param {!Network.NetworkRequestNode} node
   * @return {boolean}
   */
  _applyFilter(node) {
    const request = node.request();
    if (this._timeFilter && !this._timeFilter(request))
      return false;
    const categoryName = request.resourceType().category().title;
    if (!this._resourceCategoryFilterUI.accept(categoryName))
      return false;
    if (this._dataURLFilterUI.checked() && request.parsedURL.isDataURL())
      return false;
    if (request.statusText === 'Service Worker Fallback Required')
      return false;
    for (let i = 0; i < this._filters.length; ++i) {
      if (!this._filters[i](request))
        return false;
    }
    return true;
  }

  /**
   * @param {string} query
   */
  _parseFilterQuery(query) {
    const descriptors = this._filterParser.parse(query);
    this._filters = descriptors.map(descriptor => {
      const key = descriptor.key;
      const text = descriptor.text || '';
      const regex = descriptor.regex;
      let filter;
      if (key) {
        const defaultText = (key + ':' + text).escapeForRegExp();
        filter = this._createSpecialFilter(/** @type {!Network.NetworkLogView.FilterType} */ (key), text) ||
            Network.NetworkLogView._requestPathFilter.bind(null, new RegExp(defaultText, 'i'));
      } else if (descriptor.regex) {
        filter = Network.NetworkLogView._requestPathFilter.bind(null, /** @type {!RegExp} */ (regex));
      } else {
        filter = Network.NetworkLogView._requestPathFilter.bind(null, new RegExp(text.escapeForRegExp(), 'i'));
      }
      return descriptor.negative ? Network.NetworkLogView._negativeFilter.bind(null, filter) : filter;
    });
  }

  /**
   * @param {!Network.NetworkLogView.FilterType} type
   * @param {string} value
   * @return {?Network.NetworkLogView.Filter}
   */
  _createSpecialFilter(type, value) {
    switch (type) {
      case Network.NetworkLogView.FilterType.Domain:
        return Network.NetworkLogView._createRequestDomainFilter(value);

      case Network.NetworkLogView.FilterType.HasResponseHeader:
        return Network.NetworkLogView._requestResponseHeaderFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.Is:
        if (value.toLowerCase() === Network.NetworkLogView.IsFilterType.Running)
          return Network.NetworkLogView._runningRequestFilter;
        if (value.toLowerCase() === Network.NetworkLogView.IsFilterType.FromCache)
          return Network.NetworkLogView._fromCacheRequestFilter;
        break;

      case Network.NetworkLogView.FilterType.LargerThan:
        return this._createSizeFilter(value.toLowerCase());

      case Network.NetworkLogView.FilterType.Method:
        return Network.NetworkLogView._requestMethodFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.MimeType:
        return Network.NetworkLogView._requestMimeTypeFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.MixedContent:
        return Network.NetworkLogView._requestMixedContentFilter.bind(
            null, /** @type {!Network.NetworkLogView.MixedContentFilterValues} */ (value));

      case Network.NetworkLogView.FilterType.Scheme:
        return Network.NetworkLogView._requestSchemeFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.SetCookieDomain:
        return Network.NetworkLogView._requestSetCookieDomainFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.SetCookieName:
        return Network.NetworkLogView._requestSetCookieNameFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.SetCookieValue:
        return Network.NetworkLogView._requestSetCookieValueFilter.bind(null, value);

      case Network.NetworkLogView.FilterType.Priority:
        return Network.NetworkLogView._requestPriorityFilter.bind(null, PerfUI.uiLabelToNetworkPriority(value));

      case Network.NetworkLogView.FilterType.StatusCode:
        return Network.NetworkLogView._statusCodeFilter.bind(null, value);
    }
    return null;
  }

  /**
   * @param {string} value
   * @return {?Network.NetworkLogView.Filter}
   */
  _createSizeFilter(value) {
    let multiplier = 1;
    if (value.endsWith('k')) {
      multiplier = 1024;
      value = value.substring(0, value.length - 1);
    } else if (value.endsWith('m')) {
      multiplier = 1024 * 1024;
      value = value.substring(0, value.length - 1);
    }
    const quantity = Number(value);
    if (isNaN(quantity))
      return null;
    return Network.NetworkLogView._requestSizeLargerThanFilter.bind(null, quantity * multiplier);
  }

  _filterRequests() {
    this._removeAllHighlights();
    this._invalidateAllItems();
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {?Network.NetworkRequestNode}
   */
  _reveal(request) {
    this.removeAllNodeHighlights();
    const node = request[Network.NetworkLogView._networkNodeSymbol];
    if (!node || !node.dataGrid)
      return null;
    node.reveal();
    return node;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  revealAndHighlightRequest(request) {
    const node = this._reveal(request);
    if (node)
      this._highlightNode(node);
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  selectRequest(request) {
    this.setTextFilterValue('');
    const node = this._reveal(request);
    if (node)
      node.select();
  }

  removeAllNodeHighlights() {
    if (this._highlightedNode) {
      this._highlightedNode.element().classList.remove('highlighted-row');
      this._highlightedNode = null;
    }
  }

  /**
   * @param {!Network.NetworkRequestNode} node
   */
  _highlightNode(node) {
    UI.runCSSAnimationOnce(node.element(), 'highlighted-row');
    this._highlightedNode = node;
  }

  /**
   * @param {!Array<!SDK.NetworkRequest>} requests
   * @return {!Array<!SDK.NetworkRequest>}
   */
  _filterOutBlobRequests(requests) {
    return requests.filter(request => !request.isBlobRequest());
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {!Promise<string>}
   */
  async _generateFetchCall(request) {
    const ignoredHeaders = {
      // Internal headers
      'method': 1,
      'path': 1,
      'scheme': 1,
      'version': 1,

      // Unsafe headers
      // Keep this list synchronized with src/net/http/http_util.cc
      'accept-charset': 1,
      'accept-encoding': 1,
      'access-control-request-headers': 1,
      'access-control-request-method': 1,
      'connection': 1,
      'content-length': 1,
      'cookie': 1,
      'cookie2': 1,
      'date': 1,
      'dnt': 1,
      'expect': 1,
      'host': 1,
      'keep-alive': 1,
      'origin': 1,
      'referer': 1,
      'te': 1,
      'trailer': 1,
      'transfer-encoding': 1,
      'upgrade': 1,
      'via': 1,
      // TODO(phistuck) - remove this once crbug.com/571722 is fixed.
      'user-agent': 1
    };

    const credentialHeaders = {'cookie': 1, 'authorization': 1};

    const url = JSON.stringify(request.url());

    const requestHeaders = request.requestHeaders();
    const headerData = requestHeaders.reduce((result, header) => {
      const name = header.name;

      if (!ignoredHeaders[name.toLowerCase()] && !name.includes(':'))
        result.append(name, header.value);

      return result;
    }, new Headers());

    const headers = {};
    for (const headerArray of headerData)
      headers[headerArray[0]] = headerArray[1];

    const credentials =
        request.requestCookies || requestHeaders.some(({name}) => credentialHeaders[name.toLowerCase()]) ? 'include' :
                                                                                                           'omit';

    const referrerHeader = requestHeaders.find(({name}) => name.toLowerCase() === 'referer');

    const referrer = referrerHeader ? referrerHeader.value : void 0;

    const referrerPolicy = request.referrerPolicy() || void 0;

    const requestBody = await request.requestFormData();

    const fetchOptions = {
      credentials,
      headers: Object.keys(headers).length ? headers : void 0,
      referrer,
      referrerPolicy,
      body: requestBody,
      method: request.requestMethod,
      mode: 'cors'
    };

    const options = JSON.stringify(fetchOptions);
    return `fetch(${url}, ${options});`;
  }

  /**
   * @param {!Array<!SDK.NetworkRequest>} requests
   * @return {!Promise<string>}
   */
  async _generateAllFetchCall(requests) {
    const nonBlobRequests = this._filterOutBlobRequests(requests);
    const commands = await Promise.all(nonBlobRequests.map(request => this._generateFetchCall(request)));
    return commands.join(' ;\n');
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {string} platform
   * @return {!Promise<string>}
   */
  async _generateCurlCommand(request, platform) {
    let command = ['curl'];
    // These headers are derived from URL (except "version") and would be added by cURL anyway.
    const ignoredHeaders = {'host': 1, 'method': 1, 'path': 1, 'scheme': 1, 'version': 1};

    function escapeStringWin(str) {
      /* If there are no new line characters do not escape the " characters
               since it only uglifies the command.

               Because cmd.exe parser and MS Crt arguments parsers use some of the
               same escape characters, they can interact with each other in
               horrible ways, the order of operations is critical.

               Replace \ with \\ first because it is an escape character for certain
               conditions in both parsers.

               Replace all " with \" to ensure the first parser does not remove it.

               Then escape all characters we are not sure about with ^ to ensure it
               gets to MS Crt parser safely.

               The % character is special because MS Crt parser will try and look for
               ENV variables and fill them in it's place. We cannot escape them with %
               and cannot escape them with ^ (because it's cmd.exe's escape not MS Crt
               parser); So we can get cmd.exe parser to escape the character after it,
               if it is followed by a valid beginning character of an ENV variable.
               This ensures we do not try and double escape another ^ if it was placed
               by the previous replace.

               Lastly we replace new lines with ^ and TWO new lines because the first
               new line is there to enact the escape command the second is the character
               to escape (in this case new line).
            */
      const encapsChars = /[\r\n]/.test(str) ? '^"' : '"';
      return encapsChars +
          str.replace(/\\/g, '\\\\')
              .replace(/"/g, '\\"')
              .replace(/[^a-zA-Z0-9\s_\-:=+~'\/.',?;()*`]/g, '^$&')
              .replace(/%(?=[a-zA-Z0-9_])/g, '%^')
              .replace(/\r\n|[\n\r]/g, '^\n\n') +
          encapsChars;
    }

    /**
     * @param {string} str
     * @return {string}
     */
    function escapeStringPosix(str) {
      /**
       * @param {string} x
       * @return {string}
       */
      function escapeCharacter(x) {
        const code = x.charCodeAt(0);
        // Add leading zero when needed to not care about the next character.
        return code < 16 ? '\\u0' + code.toString(16) : '\\u' + code.toString(16);
      }

      if (/[\u0000-\u001f\u007f-\u009f]|\'/.test(str)) {
        // Use ANSI-C quoting syntax.
        return '$\'' +
            str.replace(/\\/g, '\\\\')
                .replace(/\'/g, '\\\'')
                .replace(/\n/g, '\\n')
                .replace(/\r/g, '\\r')
                .replace(/[\u0000-\u001f\u007f-\u009f]/g, escapeCharacter) +
            '\'';
      } else {
        // Use single quote syntax.
        return '\'' + str + '\'';
      }
    }

    // cURL command expected to run on the same platform that DevTools run
    // (it may be different from the inspected page platform).
    const escapeString = platform === 'win' ? escapeStringWin : escapeStringPosix;

    command.push(escapeString(request.url()).replace(/[[{}\]]/g, '\\$&'));

    let inferredMethod = 'GET';
    const data = [];
    const requestContentType = request.requestContentType();
    const formData = await request.requestFormData();
    if (requestContentType && requestContentType.startsWith('application/x-www-form-urlencoded') && formData) {
      data.push('--data');
      data.push(escapeString(formData));
      ignoredHeaders['content-length'] = true;
      inferredMethod = 'POST';
    } else if (formData) {
      data.push('--data-binary');
      data.push(escapeString(formData));
      ignoredHeaders['content-length'] = true;
      inferredMethod = 'POST';
    }

    if (request.requestMethod !== inferredMethod) {
      command.push('-X');
      command.push(request.requestMethod);
    }

    const requestHeaders = request.requestHeaders();
    for (let i = 0; i < requestHeaders.length; i++) {
      const header = requestHeaders[i];
      const name = header.name.replace(/^:/, '');  // Translate SPDY v3 headers to HTTP headers.
      if (name.toLowerCase() in ignoredHeaders)
        continue;
      command.push('-H');
      command.push(escapeString(name + ': ' + header.value));
    }
    command = command.concat(data);
    command.push('--compressed');

    if (request.securityState() === Protocol.Security.SecurityState.Insecure)
      command.push('--insecure');
    return command.join(' ');
  }

  /**
   * @param {!Array<!SDK.NetworkRequest>} requests
   * @param {string} platform
   * @return {!Promise<string>}
   */
  async _generateAllCurlCommand(requests, platform) {
    const nonBlobRequests = this._filterOutBlobRequests(requests);
    const commands = await Promise.all(nonBlobRequests.map(request => this._generateCurlCommand(request, platform)));
    if (platform === 'win')
      return commands.join(' &\r\n');
    else
      return commands.join(' ;\n');
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {!Promise<string>}
   */
  async _generatePowerShellCommand(request) {
    const command = ['Invoke-WebRequest'];
    const ignoredHeaders =
        new Set(['host', 'connection', 'proxy-connection', 'content-length', 'expect', 'range', 'content-type']);

    /**
     * @param {string} str
     * @return {string}
     */
    function escapeString(str) {
      return '"' +
          str.replace(/[`\$"]/g, '`$&').replace(/[^\x20-\x7E]/g, char => '$([char]' + char.charCodeAt(0) + ')') + '"';
    }

    command.push('-Uri');
    command.push(escapeString(request.url()));

    if (request.requestMethod !== 'GET') {
      command.push('-Method');
      command.push(escapeString(request.requestMethod));
    }

    const requestHeaders = request.requestHeaders();
    const headerNameValuePairs = [];
    for (const header of requestHeaders) {
      const name = header.name.replace(/^:/, '');  // Translate h2 headers to HTTP headers.
      if (ignoredHeaders.has(name.toLowerCase()))
        continue;
      headerNameValuePairs.push(escapeString(name) + '=' + escapeString(header.value));
    }
    if (headerNameValuePairs.length) {
      command.push('-Headers');
      command.push('@{' + headerNameValuePairs.join('; ') + '}');
    }

    const contentTypeHeader = requestHeaders.find(({name}) => name.toLowerCase() === 'content-type');
    if (contentTypeHeader) {
      command.push('-ContentType');
      command.push(escapeString(contentTypeHeader.value));
    }

    const formData = await request.requestFormData();
    if (formData) {
      command.push('-Body');
      const body = escapeString(formData);
      if (/[^\x20-\x7E]/.test(formData))
        command.push('([System.Text.Encoding]::UTF8.GetBytes(' + body + '))');
      else
        command.push(body);
    }

    return command.join(' ');
  }

  /**
   * @param {!Array<!SDK.NetworkRequest>} requests
   * @return {!Promise<string>}
   */
  async _generateAllPowerShellCommand(requests) {
    const nonBlobRequests = this._filterOutBlobRequests(requests);
    const commands = await Promise.all(nonBlobRequests.map(request => this._generatePowerShellCommand(request)));
    return commands.join(';\r\n');
  }
};

Network.NetworkLogView._isFilteredOutSymbol = Symbol('isFilteredOut');
Network.NetworkLogView._networkNodeSymbol = Symbol('NetworkNode');

Network.NetworkLogView.HTTPSchemas = {
  'http': true,
  'https': true,
  'ws': true,
  'wss': true
};

/** @enum {symbol} */
Network.NetworkLogView.Events = {
  RequestSelected: Symbol('RequestSelected')
};

/** @enum {string} */
Network.NetworkLogView.FilterType = {
  Domain: 'domain',
  HasResponseHeader: 'has-response-header',
  Is: 'is',
  LargerThan: 'larger-than',
  Method: 'method',
  MimeType: 'mime-type',
  MixedContent: 'mixed-content',
  Priority: 'priority',
  Scheme: 'scheme',
  SetCookieDomain: 'set-cookie-domain',
  SetCookieName: 'set-cookie-name',
  SetCookieValue: 'set-cookie-value',
  StatusCode: 'status-code'
};

/** @enum {string} */
Network.NetworkLogView.MixedContentFilterValues = {
  All: 'all',
  Displayed: 'displayed',
  Blocked: 'blocked',
  BlockOverridden: 'block-overridden'
};

/** @enum {string} */
Network.NetworkLogView.IsFilterType = {
  Running: 'running',
  FromCache: 'from-cache'
};

/** @type {!Array<string>} */
Network.NetworkLogView._searchKeys =
    Object.keys(Network.NetworkLogView.FilterType).map(key => Network.NetworkLogView.FilterType[key]);

/** @typedef {function(!SDK.NetworkRequest): boolean} */
Network.NetworkLogView.Filter;

/**
 * @interface
 */
Network.GroupLookupInterface = function() {};

Network.GroupLookupInterface.prototype = {
  /**
   * @param {!SDK.NetworkRequest} request
   * @return {?Network.NetworkGroupNode}
   */
  groupNodeForRequest: function(request) {},

  reset: function() {}
};
