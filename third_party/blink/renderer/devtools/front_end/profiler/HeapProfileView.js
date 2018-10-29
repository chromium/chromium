// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {UI.Searchable}
 * @unrestricted
 */
Profiler.HeapProfileView = class extends Profiler.ProfileView {
  /**
   * @param {!Profiler.SamplingHeapProfileHeader} profileHeader
   */
  constructor(profileHeader) {
    super();

    this._profileHeader = profileHeader;
    this._profileType = profileHeader.profileType();
    const views = [
      Profiler.ProfileView.ViewTypes.Flame, Profiler.ProfileView.ViewTypes.Heavy, Profiler.ProfileView.ViewTypes.Tree
    ];

    const isNativeProfile = this._profileType.id === Profiler.SamplingNativeHeapProfileType.TypeId ||
        this._profileType.id === Profiler.SamplingNativeHeapSnapshotType.TypeId;
    if (isNativeProfile)
      views.push(Profiler.ProfileView.ViewTypes.Text);

    this.initialize(new Profiler.HeapProfileView.NodeFormatter(this), views);
    const profile = new Profiler.SamplingHeapProfileModel(profileHeader._profile || profileHeader.protocolProfile());
    this.adjustedTotal = profile.total;
    this.setProfile(profile);

    this._selectedSizeText = new UI.ToolbarText();

    if (Runtime.experiments.isEnabled('samplingHeapProfilerTimeline')) {
      this._timelineOverview = new Profiler.HeapTimelineOverview();
      this._timelineOverview.addEventListener(
          Profiler.HeapTimelineOverview.IdsRangeChanged, this._onIdsRangeChanged.bind(this));
      this._timelineOverview.show(this.element, this.element.firstChild);

      this._profileType.addEventListener(
          Profiler.SamplingHeapProfileType.Events.StatsUpdate, this._onStatsUpdate, this);
      this._profileType.once(Profiler.ProfileType.Events.ProfileComplete).then(() => {
        this._profileType.removeEventListener(
            Profiler.SamplingHeapProfileType.Events.StatsUpdate, this._onStatsUpdate, this);
        this._timelineOverview.updateGrid();
      });
    }
  }

  /**
   * @override
   * @return {!Array<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    return [...super.syncToolbarItems(), this._selectedSizeText];
  }

  /**
   * @param {!Common.Event} event
   */
  _onIdsRangeChanged(event) {
    const minId = /** @type {number} */ (event.data.minId);
    const maxId = /** @type {number} */ (event.data.maxId);
    this._selectedSizeText.setText(ls`Selected size: ${Number.bytesToString(event.data.size)}`);
    this._setSelectionRange(minId, maxId);
  }

  /**
   * @param {number} minId
   * @param {number} maxId
   */
  _setSelectionRange(minId, maxId) {
    const profile = new Profiler.SamplingHeapProfileModel(
        this._profileHeader._profile || this._profileHeader.protocolProfile(), minId, maxId);
    this.adjustedTotal = profile.total;
    this.setProfile(profile);
  }

  /**
   * @param {!Common.Event} event
   */
  _onStatsUpdate(event) {
    const profile = event.data;

    if (!this._startTime) {
      this._startTime = Date.now();
      this._timestamps = [];
      this._sizes = [];
      this._max = [];
      this._ordinals = [];
      this._totalTime = 30000;
      this._lastOrdinal = 0;
    }

    this._sizes.fill(0);
    this._sizes.push(0);
    this._timestamps.push(Date.now() - this._startTime);
    this._ordinals.push(this._lastOrdinal + 1);
    this._lastOrdinal = profile.samples.reduce((res, sample) => Math.max(res, sample.ordinal), this._lastOrdinal);
    for (const sample of profile.samples) {
      const bucket = this._ordinals.upperBound(sample.ordinal) - 1;
      this._sizes[bucket] += sample.size;
    }
    this._max.push(this._sizes.peekLast());

    if (this._timestamps.peekLast() > this._totalTime)
      this._totalTime *= 2;

    const samples = /** @type {!Profiler.HeapTimelineOverview.Samples} */ ({
      sizes: this._sizes,
      max: this._max,
      ids: this._ordinals,
      timestamps: this._timestamps,
      totalTime: this._totalTime
    });

    this._timelineOverview.setSamples(samples);
  }

  /**
   * @override
   * @param {string} columnId
   * @return {string}
   */
  columnHeader(columnId) {
    switch (columnId) {
      case 'self':
        return Common.UIString('Self Size (bytes)');
      case 'total':
        return Common.UIString('Total Size (bytes)');
    }
    return '';
  }

  /**
   * @override
   * @return {!PerfUI.FlameChartDataProvider}
   */
  createFlameChartDataProvider() {
    return new Profiler.HeapFlameChartDataProvider(
        /** @type {!Profiler.SamplingHeapProfileModel} */ (this.profile()), this._profileHeader.heapProfilerModel());
  }

  /**
   * @override
   * @param {!UI.SimpleView} view
   */
  populateTextView(view) {
    const guides = '+!:|';
    let text = `Sampling memory profile.\n\nDate/Time:       ${new Date()}\n` +
        `Report Version:  7\n` +
        `App Version:     ${/Chrom\S*/.exec(navigator.appVersion)[0] || 'Unknown'}\n` +
        `Node Weight:     1 KiB\n` +
        `Total Size:      ${Math.round(this.profile().root.total / 1024)} KiB\n` +
        `----\n\nCall graph:\n`;
    const sortedChildren = this.profile().root.children.sort((a, b) => b.total - a.total);
    const modules = this.profile().modules.map(
        m => Object.assign({address: BigInt(m.baseAddress), endAddress: BigInt(m.baseAddress) + BigInt(m.size)}, m));
    modules.sort((m1, m2) => m1.address > m2.address ? 1 : m1.address < m2.address ? -1 : 0);
    for (const child of sortedChildren)
      printTree('    ', child !== sortedChildren.peekLast(), child);

    text += '\nBinary Images:\n';
    for (const module of modules) {
      const fileName = /[^/\\]*$/.exec(module.name)[0];
      const version = '1.0';
      const formattedUuid = module.uuid.includes('-') ?
          module.uuid :
          module.uuid.replace(/(.{8})(.{4})(.{4})(.{4})(.{12}).*/, '$1-$2-$3-$4-$5');
      text += `${('0x' + module.address.toString(16)).padStart(18)} - `;
      text += `${('0x' + (module.endAddress - BigInt(1)).toString(16)).padStart(18)}`;
      text += `  ${fileName} (${version}) <${formattedUuid}> ${module.name}\n`;
    }

    view.contentElement.createChild('pre', 'profile-text-view monospace').textContent = text;

    /**
     * @param {string} padding
     * @param {boolean} drawGuide
     * @param {!SDK.ProfileNode} node
     */
    function printTree(padding, drawGuide, node) {
      const addressText = /0x[0-9a-f]*|[0-9]*/.exec(node.functionName)[0] || '';
      let module;
      if (addressText) {
        const address = BigInt(addressText);
        const pos = modules.upperBound(address, (address, module) => address - module.address);
        if (pos > 0 && address < modules[pos - 1].endAddress)
          module = modules[pos - 1];
      }
      const functionName =
          (addressText ? node.functionName.substr(addressText.length + 1) : node.functionName) || '???';
      text += `${padding}${Math.round(node.total / 1024)}  ${functionName}  `;
      if (module) {
        const fileName = /[^/\\]*$/.exec(module.name);
        if (fileName)
          text += `(in ${fileName})  `;
        const offset = BigInt(addressText) - module.address;
        text += `load address ${module.baseAddress} + 0x${offset.toString(16)}  `;
      }
      if (addressText)
        text += `[${addressText}]`;
      text += '\n';
      const guideChar = drawGuide ? guides[padding.length / 2 % guides.length] : ' ';
      const nextPadding = padding + guideChar + ' ';
      const sortedChildren = node.children.sort((a, b) => b.total - a.total);
      for (const child of sortedChildren)
        printTree(nextPadding, child !== sortedChildren.peekLast(), child);
    }
  }
};

/**
 * @unrestricted
 */
Profiler.SamplingHeapProfileTypeBase = class extends Profiler.ProfileType {
  /**
   * @param {string} typeId
   * @param {string} description
   */
  constructor(typeId, description) {
    super(typeId, description);
    this._recording = false;
  }

  /**
   * @override
   * @return {?Profiler.SamplingHeapProfileHeader}
   */
  profileBeingRecorded() {
    return /** @type {?Profiler.SamplingHeapProfileHeader} */ (super.profileBeingRecorded());
  }

  /**
   * @override
   * @return {string}
   */
  typeName() {
    return 'Heap';
  }

  /**
   * @override
   * @return {string}
   */
  fileExtension() {
    return '.heapprofile';
  }

  get buttonTooltip() {
    return this._recording ? ls`Stop heap profiling` : ls`Start heap profiling`;
  }

  /**
   * @override
   * @return {boolean}
   */
  buttonClicked() {
    if (this._recording)
      this._stopRecordingProfile();
    else
      this._startRecordingProfile();
    return this._recording;
  }

  _startRecordingProfile() {
    const heapProfilerModel = UI.context.flavor(SDK.HeapProfilerModel);
    if (this.profileBeingRecorded() || !heapProfilerModel)
      return;
    const profileHeader = new Profiler.SamplingHeapProfileHeader(heapProfilerModel, this);
    this.setProfileBeingRecorded(profileHeader);
    this.addProfile(profileHeader);
    profileHeader.updateStatus(ls`Recording\u2026`);

    const icon = UI.Icon.create('smallicon-warning');
    icon.title = ls`Heap profiler is recording`;
    UI.inspectorView.setPanelIcon('heap_profiler', icon);

    this._recording = true;
    this._startSampling();
  }

  async _stopRecordingProfile() {
    this._recording = false;
    if (!this.profileBeingRecorded() || !this.profileBeingRecorded().heapProfilerModel())
      return;

    this.profileBeingRecorded().updateStatus(ls`Stopping\u2026`);
    const profile = await this._stopSampling();
    const recordedProfile = this.profileBeingRecorded();
    if (recordedProfile) {
      console.assert(profile);
      recordedProfile.setProtocolProfile(profile);
      recordedProfile.updateStatus('');
      this.setProfileBeingRecorded(null);
    }
    UI.inspectorView.setPanelIcon('heap_profiler', null);
    this.dispatchEventToListeners(Profiler.ProfileType.Events.ProfileComplete, recordedProfile);
  }

  /**
   * @override
   * @param {string} title
   * @return {!Profiler.ProfileHeader}
   */
  createProfileLoadedFromFile(title) {
    return new Profiler.SamplingHeapProfileHeader(null, this, title);
  }

  /**
   * @override
   */
  profileBeingRecordedRemoved() {
    this._stopRecordingProfile();
  }

  _startSampling() {
    throw 'Not implemented';
  }

  /**
   * return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  _stopSampling() {
    throw 'Not implemented';
  }
};

/**
 * @unrestricted
 */
Profiler.SamplingHeapProfileType = class extends Profiler.SamplingHeapProfileTypeBase {
  constructor() {
    super(Profiler.SamplingHeapProfileType.TypeId, ls`Allocation sampling`);
    Profiler.SamplingHeapProfileType.instance = this;
    this._updateTimer = null;
    this._updateIntervalMs = 200;
  }

  get treeItemTitle() {
    return ls`SAMPLING PROFILES`;
  }

  get description() {
    return ls`Record memory allocations using sampling method.
              This profile type has minimal performance overhead and can be used for long running operations.
              It provides good approximation of allocations broken down by JavaScript execution stack.`;
  }

  /**
   * @override
   * @return {boolean}
   */
  hasTemporaryView() {
    return Runtime.experiments.isEnabled('samplingHeapProfilerTimeline');
  }

  /**
   * @override
   */
  _startSampling() {
    this.profileBeingRecorded().heapProfilerModel().startSampling();
    if (Runtime.experiments.isEnabled('samplingHeapProfilerTimeline'))
      this._updateTimer = setTimeout(this._updateStats.bind(this), this._updateIntervalMs);
  }

  /**
   * @override
   * return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  _stopSampling() {
    clearTimeout(this._updateTimer);
    this._updateTimer = null;
    this.dispatchEventToListeners(Profiler.SamplingHeapProfileType.Events.RecordingStopped);
    return this.profileBeingRecorded().heapProfilerModel().stopSampling();
  }

  async _updateStats() {
    const profile = await this.profileBeingRecorded().heapProfilerModel().getSamplingProfile();
    if (!this._updateTimer)
      return;
    this.dispatchEventToListeners(Profiler.SamplingHeapProfileType.Events.StatsUpdate, profile);
    this._updateTimer = setTimeout(this._updateStats.bind(this), this._updateIntervalMs);
  }
};

Profiler.SamplingHeapProfileType.TypeId = 'SamplingHeap';

/** @enum {symbol} */
Profiler.SamplingHeapProfileType.Events = {
  RecordingStopped: Symbol('RecordingStopped'),
  StatsUpdate: Symbol('StatsUpdate')
};

/**
 * @unrestricted
 */
Profiler.SamplingNativeHeapProfileType = class extends Profiler.SamplingHeapProfileTypeBase {
  constructor() {
    super(Profiler.SamplingNativeHeapProfileType.TypeId, ls`Native memory allocation sampling`);
    Profiler.SamplingNativeHeapProfileType.instance = this;
  }

  get treeItemTitle() {
    return ls`NATIVE SAMPLING PROFILES`;
  }

  get description() {
    return ls`Allocation profiles show sampled native memory allocations from the renderer process.`;
  }

  /**
   * @override
   */
  _startSampling() {
    this.profileBeingRecorded().heapProfilerModel().startNativeSampling();
  }

  /**
   * @override
   * return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  _stopSampling() {
    return this.profileBeingRecorded().heapProfilerModel().stopNativeSampling();
  }
};

Profiler.SamplingNativeHeapProfileType.TypeId = 'SamplingNativeHeapRecording';

/**
 * @unrestricted
 */
Profiler.SamplingNativeHeapSnapshotType = class extends Profiler.SamplingHeapProfileTypeBase {
  /**
   * @param {string} processType
   */
  constructor(processType) {
    super(Profiler.SamplingNativeHeapSnapshotType.TypeId, ls`Native memory allocation snapshot (${processType})`);
  }

  /**
   * @override
   * @return {boolean}
   */
  isInstantProfile() {
    return true;
  }

  get treeItemTitle() {
    return ls`NATIVE SNAPSHOTS`;
  }

  get description() {
    return ls`Native memory snapshots show sampled native allocations in the renderer process since start up.
              Chrome has to be started with --sampling-heap-profiler flag.
              Check flags at chrome://flags`;
  }

  /**
   * @override
   * @return {boolean}
   */
  buttonClicked() {
    this._takeSnapshot();
    return false;
  }

  /**
   * @return {!Promise}
   */
  async _takeSnapshot() {
    if (this.profileBeingRecorded())
      return;
    const heapProfilerModel = UI.context.flavor(SDK.HeapProfilerModel);
    if (!heapProfilerModel)
      return;

    const profile =
        new Profiler.SamplingHeapProfileHeader(heapProfilerModel, this, ls`Snapshot ${this.nextProfileUid()}`);
    this.setProfileBeingRecorded(profile);
    this.addProfile(profile);
    profile.updateStatus(ls`Snapshotting\u2026`);

    const protocolProfile = await this._takeNativeSnapshot(/** @type {!SDK.HeapProfilerModel} */ (heapProfilerModel));
    const recordedProfile = this.profileBeingRecorded();
    if (recordedProfile) {
      console.assert(protocolProfile);
      recordedProfile.setProtocolProfile(protocolProfile);
      recordedProfile.updateStatus('');
      this.setProfileBeingRecorded(null);
    }

    this.dispatchEventToListeners(Profiler.ProfileType.Events.ProfileComplete, recordedProfile);
  }

  /**
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   * @return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  _takeNativeSnapshot(heapProfilerModel) {
    throw 'Not implemented';
  }
};

Profiler.SamplingNativeHeapSnapshotType.TypeId = 'SamplingNativeHeapSnapshot';

Profiler.SamplingNativeHeapSnapshotBrowserType = class extends Profiler.SamplingNativeHeapSnapshotType {
  constructor() {
    super(ls`Browser`);
    Profiler.SamplingNativeHeapSnapshotBrowserType.instance = this;
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   * @return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  async _takeNativeSnapshot(heapProfilerModel) {
    return await heapProfilerModel.takeNativeBrowserSnapshot();
  }
};

Profiler.SamplingNativeHeapSnapshotRendererType = class extends Profiler.SamplingNativeHeapSnapshotType {
  constructor() {
    super(ls`Renderer`);
    Profiler.SamplingNativeHeapSnapshotRendererType.instance = this;
  }

  /**
   * @override
   * @param {!SDK.HeapProfilerModel} heapProfilerModel
   * @return {!Promise<!Protocol.HeapProfiler.SamplingHeapProfile>}
   */
  async _takeNativeSnapshot(heapProfilerModel) {
    return await heapProfilerModel.takeNativeSnapshot();
  }
};

/**
 * @unrestricted
 */
Profiler.SamplingHeapProfileHeader = class extends Profiler.WritableProfileHeader {
  /**
   * @param {?SDK.HeapProfilerModel} heapProfilerModel
   * @param {!Profiler.SamplingHeapProfileTypeBase} type
   * @param {string=} title
   */
  constructor(heapProfilerModel, type, title) {
    super(
        heapProfilerModel && heapProfilerModel.debuggerModel(), type,
        title || Common.UIString('Profile %d', type.nextProfileUid()));
    this._heapProfilerModel = heapProfilerModel;
    this._protocolProfile =
        /** @type {!Protocol.HeapProfiler.SamplingHeapProfile} */ ({head: {callFrame: {}, children: []}});
  }

  /**
   * @override
   * @return {!Profiler.ProfileView}
   */
  createView() {
    return new Profiler.HeapProfileView(this);
  }

  /**
   * @return {!Protocol.HeapProfiler.SamplingHeapProfile}
   */
  protocolProfile() {
    return this._protocolProfile;
  }

  /**
   * @return {?SDK.HeapProfilerModel}
   */
  heapProfilerModel() {
    return this._heapProfilerModel;
  }
};

/**
 * @unrestricted
 */
Profiler.SamplingHeapProfileNode = class extends SDK.ProfileNode {
  /**
   * @param {!Protocol.HeapProfiler.SamplingHeapProfileNode} node
   */
  constructor(node) {
    const callFrame = node.callFrame || /** @type {!Protocol.Runtime.CallFrame} */ ({
                        // Backward compatibility for old CpuProfileNode format.
                        functionName: node['functionName'],
                        scriptId: node['scriptId'],
                        url: node['url'],
                        lineNumber: node['lineNumber'] - 1,
                        columnNumber: node['columnNumber'] - 1
                      });
    super(callFrame);
    this.self = node.selfSize;
  }
};

/**
 * @unrestricted
 */
Profiler.SamplingHeapProfileModel = class extends SDK.ProfileTreeModel {
  /**
   * @param {!Protocol.HeapProfiler.SamplingHeapProfile} profile
   * @param {number=} minOrdinal
   * @param {number=} maxOrdinal
   */
  constructor(profile, minOrdinal, maxOrdinal) {
    super();
    this.modules = profile.modules || [];

    /** @type {?Map<number, number>} */
    let nodeIdToSizeMap = null;
    if (minOrdinal || maxOrdinal) {
      nodeIdToSizeMap = new Map();
      minOrdinal = minOrdinal || 0;
      maxOrdinal = maxOrdinal || Infinity;
      for (const sample of profile.samples) {
        if (sample.ordinal < minOrdinal || sample.ordinal > maxOrdinal)
          continue;
        const size = nodeIdToSizeMap.get(sample.nodeId) || 0;
        nodeIdToSizeMap.set(sample.nodeId, size + sample.size);
      }
    }

    this.initialize(translateProfileTree(profile.head));

    /**
     * @param {!Protocol.HeapProfiler.SamplingHeapProfileNode} root
     * @return {!Profiler.SamplingHeapProfileNode}
     */
    function translateProfileTree(root) {
      const resultRoot = new Profiler.SamplingHeapProfileNode(root);
      const sourceNodeStack = [root];
      const targetNodeStack = [resultRoot];
      while (sourceNodeStack.length) {
        const sourceNode = sourceNodeStack.pop();
        const targetNode = targetNodeStack.pop();
        targetNode.children = sourceNode.children.map(child => {
          const targetChild = new Profiler.SamplingHeapProfileNode(child);
          if (nodeIdToSizeMap)
            targetChild.self = nodeIdToSizeMap.get(child.id) || 0;
          return targetChild;
        });
        sourceNodeStack.pushAll(sourceNode.children);
        targetNodeStack.pushAll(targetNode.children);
      }
      pruneEmptyBranches(resultRoot);
      return resultRoot;
    }

    /**
     * @param {!SDK.ProfileNode} node
     * @return {boolean}
     */
    function pruneEmptyBranches(node) {
      node.children = node.children.filter(pruneEmptyBranches);
      return !!(node.children.length || node.self);
    }
  }
};

/**
 * @implements {Profiler.ProfileDataGridNode.Formatter}
 * @unrestricted
 */
Profiler.HeapProfileView.NodeFormatter = class {
  /**
   * @param {!Profiler.HeapProfileView} profileView
   */
  constructor(profileView) {
    this._profileView = profileView;
  }

  /**
   * @override
   * @param {number} value
   * @return {string}
   */
  formatValue(value) {
    return Number.withThousandsSeparator(value);
  }

  /**
   * @override
   * @param {number} value
   * @param {!Profiler.ProfileDataGridNode} node
   * @return {string}
   */
  formatPercent(value, node) {
    return Common.UIString('%.2f\xa0%%', value);
  }

  /**
   * @override
   * @param  {!Profiler.ProfileDataGridNode} node
   * @return {?Element}
   */
  linkifyNode(node) {
    const heapProfilerModel = this._profileView._profileHeader.heapProfilerModel();
    return this._profileView.linkifier().maybeLinkifyConsoleCallFrame(
        heapProfilerModel ? heapProfilerModel.target() : null, node.profileNode.callFrame, 'profile-node-file');
  }
};

/**
 * @unrestricted
 */
Profiler.HeapFlameChartDataProvider = class extends Profiler.ProfileFlameChartDataProvider {
  /**
   * @param {!SDK.ProfileTreeModel} profile
   * @param {?SDK.HeapProfilerModel} heapProfilerModel
   */
  constructor(profile, heapProfilerModel) {
    super();
    this._profile = profile;
    this._heapProfilerModel = heapProfilerModel;
  }

  /**
   * @override
   * @return {number}
   */
  minimumBoundary() {
    return 0;
  }

  /**
   * @override
   * @return {number}
   */
  totalTime() {
    return this._profile.root.total;
  }

  /**
   * @override
   * @param {number} value
   * @param {number=} precision
   * @return {string}
   */
  formatValue(value, precision) {
    return Common.UIString('%s\xa0KB', Number.withThousandsSeparator(value / 1e3));
  }

  /**
   * @override
   * @return {!PerfUI.FlameChart.TimelineData}
   */
  _calculateTimelineData() {
    /**
     * @param  {!SDK.ProfileNode} node
     * @return {number}
     */
    function nodesCount(node) {
      return node.children.reduce((count, node) => count + nodesCount(node), 1);
    }
    const count = nodesCount(this._profile.root);
    /** @type {!Array<!SDK.ProfileNode>} */
    const entryNodes = new Array(count);
    const entryLevels = new Uint16Array(count);
    const entryTotalTimes = new Float32Array(count);
    const entryStartTimes = new Float64Array(count);
    let depth = 0;
    let maxDepth = 0;
    let position = 0;
    let index = 0;

    /**
     * @param {!SDK.ProfileNode} node
     */
    function addNode(node) {
      const start = position;
      entryNodes[index] = node;
      entryLevels[index] = depth;
      entryTotalTimes[index] = node.total;
      entryStartTimes[index] = position;
      ++index;
      ++depth;
      node.children.forEach(addNode);
      --depth;
      maxDepth = Math.max(maxDepth, depth);
      position = start + node.total;
    }
    addNode(this._profile.root);

    this._maxStackDepth = maxDepth + 1;
    this._entryNodes = entryNodes;
    this._timelineData = new PerfUI.FlameChart.TimelineData(entryLevels, entryTotalTimes, entryStartTimes, null);

    return this._timelineData;
  }

  /**
   * @override
   * @param {number} entryIndex
   * @return {?Element}
   */
  prepareHighlightedEntryInfo(entryIndex) {
    const node = this._entryNodes[entryIndex];
    if (!node)
      return null;
    const entryInfo = [];
    /**
     * @param {string} title
     * @param {string} value
     */
    function pushEntryInfoRow(title, value) {
      entryInfo.push({title: title, value: value});
    }
    pushEntryInfoRow(Common.UIString('Name'), UI.beautifyFunctionName(node.functionName));
    pushEntryInfoRow(Common.UIString('Self size'), Number.bytesToString(node.self));
    pushEntryInfoRow(Common.UIString('Total size'), Number.bytesToString(node.total));
    const linkifier = new Components.Linkifier();
    const link = linkifier.maybeLinkifyConsoleCallFrame(
        this._heapProfilerModel ? this._heapProfilerModel.target() : null, node.callFrame);
    if (link)
      pushEntryInfoRow(Common.UIString('URL'), link.textContent);
    linkifier.dispose();
    return Profiler.ProfileView.buildPopoverTable(entryInfo);
  }
};
