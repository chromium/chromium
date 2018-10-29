/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @unrestricted
 */
Bindings.BreakpointManager = class extends Common.Object {
  /**
   * @param {!Workspace.Workspace} workspace
   * @param {!SDK.TargetManager} targetManager
   * @param {!Bindings.DebuggerWorkspaceBinding} debuggerWorkspaceBinding
   */
  constructor(workspace, targetManager, debuggerWorkspaceBinding) {
    super();
    this._storage = new Bindings.BreakpointManager.Storage();
    this._workspace = workspace;
    this._targetManager = targetManager;
    this._debuggerWorkspaceBinding = debuggerWorkspaceBinding;

    /** @type {!Map<!Workspace.UISourceCode, !Map<string, !Bindings.BreakpointManager.BreakpointLocation>>} */
    this._breakpointsForUISourceCode = new Map();
    /** @type {!Map<string, !Bindings.BreakpointManager.Breakpoint>} */
    this._breakpointByStorageId = new Map();

    this._workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, this._uiSourceCodeAdded, this);
  }

  /**
   * @param {string} url
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {string}
   */
  static _breakpointStorageId(url, lineNumber, columnNumber) {
    if (!url)
      return '';
    return url + ':' + lineNumber + ':' + columnNumber;
  }

  /**
   * @param {string} fromURL
   * @param {!Workspace.UISourceCode} toSourceCode
   */
  copyBreakpoints(fromURL, toSourceCode) {
    const breakpointItems = this._storage.breakpointItems(fromURL);
    for (const item of breakpointItems)
      this.setBreakpoint(toSourceCode, item.lineNumber, item.columnNumber, item.condition, item.enabled);
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  _restoreBreakpoints(uiSourceCode) {
    const url = uiSourceCode.url();
    if (!url)
      return;

    this._storage.mute();
    const breakpointItems = this._storage.breakpointItems(url);
    for (const item of breakpointItems)
      this._innerSetBreakpoint(uiSourceCode, item.lineNumber, item.columnNumber, item.condition, item.enabled);
    this._storage.unmute();
  }

  /**
   * @param {!Common.Event} event
   */
  _uiSourceCodeAdded(event) {
    const uiSourceCode = /** @type {!Workspace.UISourceCode} */ (event.data);
    this._restoreBreakpoints(uiSourceCode);
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   * @param {boolean} enabled
   * @return {!Bindings.BreakpointManager.Breakpoint}
   */
  setBreakpoint(uiSourceCode, lineNumber, columnNumber, condition, enabled) {
    let uiLocation = new Workspace.UILocation(uiSourceCode, lineNumber, columnNumber);
    const normalizedLocation = this._debuggerWorkspaceBinding.normalizeUILocation(uiLocation);
    if (normalizedLocation.id() !== uiLocation.id()) {
      Common.Revealer.reveal(normalizedLocation);
      uiLocation = normalizedLocation;
    }
    return this._innerSetBreakpoint(
        uiLocation.uiSourceCode, uiLocation.lineNumber, uiLocation.columnNumber, condition, enabled);
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   * @param {boolean} enabled
   * @return {!Bindings.BreakpointManager.Breakpoint}
   */
  _innerSetBreakpoint(uiSourceCode, lineNumber, columnNumber, condition, enabled) {
    const itemId = Bindings.BreakpointManager._breakpointStorageId(uiSourceCode.url(), lineNumber, columnNumber);
    let breakpoint = this._breakpointByStorageId.get(itemId);
    if (breakpoint) {
      breakpoint._updateState(condition, enabled);
      breakpoint.setPrimaryUISourceCode(uiSourceCode);
      breakpoint._updateBreakpoint();
      return breakpoint;
    }
    breakpoint = new Bindings.BreakpointManager.Breakpoint(
        this, uiSourceCode, uiSourceCode.url(), lineNumber, columnNumber, condition, enabled);
    this._breakpointByStorageId.set(itemId, breakpoint);
    return breakpoint;
  }

  /**
   * @param {!Workspace.UILocation} uiLocation
   * @return {?Bindings.BreakpointManager.BreakpointLocation}
   */
  findBreakpoint(uiLocation) {
    const breakpoints = this._breakpointsForUISourceCode.get(uiLocation.uiSourceCode);
    return breakpoints ? (breakpoints.get(uiLocation.id())) || null : null;
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {!TextUtils.TextRange} textRange
   * @return {!Promise<!Array<!Workspace.UILocation>>}
   */
  possibleBreakpoints(uiSourceCode, textRange) {
    const startLocations = Bindings.debuggerWorkspaceBinding.uiLocationToRawLocations(
        uiSourceCode, textRange.startLine, textRange.startColumn);
    const endLocations = Bindings.debuggerWorkspaceBinding.uiLocationToRawLocations(
        uiSourceCode, textRange.endLine, textRange.endColumn);
    const endLocationByModel = new Map();
    for (const location of endLocations)
      endLocationByModel.set(location.debuggerModel, location);
    let startLocation = null;
    let endLocation = null;
    for (const location of startLocations) {
      const endLocationCandidate = endLocationByModel.get(location.debuggerModel);
      if (endLocationCandidate) {
        startLocation = location;
        endLocation = endLocationCandidate;
        break;
      }
    }
    if (!startLocation || !endLocation)
      return Promise.resolve([]);

    return startLocation.debuggerModel
        .getPossibleBreakpoints(startLocation, endLocation, /* restrictToFunction */ false)
        .then(toUILocations.bind(this));

    /**
     * @this {!Bindings.BreakpointManager}
     * @param {!Array<!SDK.DebuggerModel.BreakLocation>} locations
     * @return {!Array<!Workspace.UILocation>}
     */
    function toUILocations(locations) {
      let sortedLocations = locations.map(location => this._debuggerWorkspaceBinding.rawLocationToUILocation(location));
      sortedLocations = sortedLocations.filter(location => location && location.uiSourceCode === uiSourceCode);
      sortedLocations.sort(Workspace.UILocation.comparator);
      if (!sortedLocations.length)
        return [];
      const result = [sortedLocations[0]];
      let lastLocation = sortedLocations[0];
      for (let i = 1; i < sortedLocations.length; ++i) {
        if (sortedLocations[i].id() === lastLocation.id())
          continue;
        result.push(sortedLocations[i]);
        lastLocation = sortedLocations[i];
      }
      return result;
    }
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {!Array<!Bindings.BreakpointManager.BreakpointLocation>}
   */
  breakpointLocationsForUISourceCode(uiSourceCode) {
    const breakpoints = this._breakpointsForUISourceCode.get(uiSourceCode);
    return breakpoints ? Array.from(breakpoints.values()) : [];
  }

  /**
   * @return {!Array<!Bindings.BreakpointManager.BreakpointLocation>}
   */
  allBreakpointLocations() {
    let result = [];
    for (const breakpoints of this._breakpointsForUISourceCode.values())
      result = result.concat(Array.from(breakpoints.values()));
    return result;
  }

  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   * @param {boolean} removeFromStorage
   */
  _removeBreakpoint(breakpoint, removeFromStorage) {
    if (removeFromStorage)
      this._storage._removeBreakpoint(breakpoint);
    this._breakpointByStorageId.delete(breakpoint._breakpointStorageId());
  }

  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   * @param {!Workspace.UILocation} uiLocation
   */
  _uiLocationAdded(breakpoint, uiLocation) {
    let breakpoints = this._breakpointsForUISourceCode.get(uiLocation.uiSourceCode);
    if (!breakpoints) {
      breakpoints = new Map();
      this._breakpointsForUISourceCode.set(uiLocation.uiSourceCode, breakpoints);
    }
    const breakpointLocation = {breakpoint: breakpoint, uiLocation: uiLocation};
    breakpoints.set(uiLocation.id(), breakpointLocation);
    this.dispatchEventToListeners(Bindings.BreakpointManager.Events.BreakpointAdded, breakpointLocation);
  }

  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   * @param {!Workspace.UILocation} uiLocation
   */
  _uiLocationRemoved(breakpoint, uiLocation) {
    const breakpoints = this._breakpointsForUISourceCode.get(uiLocation.uiSourceCode);
    if (!breakpoints)
      return;
    const breakpointLocation = breakpoints.get(uiLocation.id()) || null;
    if (!breakpointLocation)
      return;
    breakpoints.delete(uiLocation.id());
    if (breakpoints.size === 0)
      this._breakpointsForUISourceCode.delete(uiLocation.uiSourceCode);
    this.dispatchEventToListeners(
        Bindings.BreakpointManager.Events.BreakpointRemoved, {breakpoint: breakpoint, uiLocation: uiLocation});
  }
};

/** @enum {symbol} */
Bindings.BreakpointManager.Events = {
  BreakpointAdded: Symbol('breakpoint-added'),
  BreakpointRemoved: Symbol('breakpoint-removed')
};

/** @typedef {{
 *    breakpoint: !Bindings.BreakpointManager.Breakpoint,
 *    uiLocation: !Workspace.UILocation
 *  }}
 */
Bindings.BreakpointManager.BreakpointLocation;

/**
 * @unrestricted
 * @implements {SDK.SDKModelObserver<!SDK.DebuggerModel>}
 */
Bindings.BreakpointManager.Breakpoint = class {
  /**
   * @param {!Bindings.BreakpointManager} breakpointManager
   * @param {!Workspace.UISourceCode} primaryUISourceCode
   * @param {string} url
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   * @param {boolean} enabled
   */
  constructor(breakpointManager, primaryUISourceCode, url, lineNumber, columnNumber, condition, enabled) {
    this._breakpointManager = breakpointManager;
    this._url = url;
    this._lineNumber = lineNumber;
    this._columnNumber = columnNumber;

    /** @type {?Workspace.UILocation} */
    this._defaultUILocation = null;
    /** @type {!Set<!Workspace.UILocation>} */
    this._uiLocations = new Set();

    /** @type {string} */ this._condition;
    /** @type {boolean} */ this._enabled;
    /** @type {boolean} */ this._isRemoved;

    this._currentState = null;
    /** @type {!Map.<!SDK.DebuggerModel, !Bindings.BreakpointManager.ModelBreakpoint>}*/
    this._modelBreakpoints = new Map();
    this._updateState(condition, enabled);
    this.setPrimaryUISourceCode(primaryUISourceCode);
    this._breakpointManager._targetManager.observeModels(SDK.DebuggerModel, this);
  }

  refreshInDebugger() {
    if (this._isRemoved)
      return;
    for (const breakpoint of this._modelBreakpoints.values())
      breakpoint._refreshBreakpoint();
  }

  /**
   * @override
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  modelAdded(debuggerModel) {
    const debuggerWorkspaceBinding = this._breakpointManager._debuggerWorkspaceBinding;
    this._modelBreakpoints.set(
        debuggerModel, new Bindings.BreakpointManager.ModelBreakpoint(debuggerModel, this, debuggerWorkspaceBinding));
  }

  /**
   * @override
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  modelRemoved(debuggerModel) {
    const modelBreakpoint = this._modelBreakpoints.remove(debuggerModel);
    modelBreakpoint._cleanUpAfterDebuggerIsGone();
    modelBreakpoint._removeEventListeners();
  }

  /**
   * @param {?Workspace.UISourceCode} primaryUISourceCode
   */
  setPrimaryUISourceCode(primaryUISourceCode) {
    if (this._uiLocations.size === 0 && this._defaultUILocation)
      this._breakpointManager._uiLocationRemoved(this, this._defaultUILocation);
    if (primaryUISourceCode)
      this._defaultUILocation = primaryUISourceCode.uiLocation(this._lineNumber, this._columnNumber);
    else
      this._defaultUILocation = null;
    if (this._uiLocations.size === 0 && this._defaultUILocation && !this._isRemoved)
      this._breakpointManager._uiLocationAdded(this, this._defaultUILocation);
  }

  /**
   * @return {string}
   */
  url() {
    return this._url;
  }

  /**
   * @return {number}
   */
  lineNumber() {
    return this._lineNumber;
  }

  /**
   * @return {number}
   */
  columnNumber() {
    return this._columnNumber;
  }

  /**
   * @param {!Workspace.UILocation} uiLocation
   */
  _uiLocationAdded(uiLocation) {
    if (this._isRemoved)
      return;
    if (this._uiLocations.size === 0 && this._defaultUILocation)
      this._breakpointManager._uiLocationRemoved(this, this._defaultUILocation);
    this._uiLocations.add(uiLocation);
    this._breakpointManager._uiLocationAdded(this, uiLocation);
  }

  /**
   * @param {!Workspace.UILocation} uiLocation
   */
  _uiLocationRemoved(uiLocation) {
    this._uiLocations.delete(uiLocation);
    this._breakpointManager._uiLocationRemoved(this, uiLocation);
    if (this._uiLocations.size === 0 && this._defaultUILocation && !this._isRemoved)
      this._breakpointManager._uiLocationAdded(this, this._defaultUILocation);
  }

  /**
   * @return {boolean}
   */
  enabled() {
    return this._enabled;
  }

  /**
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    this._updateState(this._condition, enabled);
  }

  /**
   * @return {string}
   */
  condition() {
    return this._condition;
  }

  /**
   * @param {string} condition
   */
  setCondition(condition) {
    this._updateState(condition, this._enabled);
  }

  /**
   * @param {string} condition
   * @param {boolean} enabled
   */
  _updateState(condition, enabled) {
    if (this._enabled === enabled && this._condition === condition)
      return;
    this._enabled = enabled;
    this._condition = condition;
    this._breakpointManager._storage._updateBreakpoint(this);
    this._updateBreakpoint();
  }

  _updateBreakpoint() {
    const modelBreakpoints = this._modelBreakpoints.valuesArray();
    for (let i = 0; i < modelBreakpoints.length; ++i)
      modelBreakpoints[i]._scheduleUpdateInDebugger();
  }

  /**
   * @param {boolean} keepInStorage
   */
  remove(keepInStorage) {
    this._isRemoved = true;
    const removeFromStorage = !keepInStorage;
    const modelBreakpoints = this._modelBreakpoints.valuesArray();
    for (let i = 0; i < modelBreakpoints.length; ++i) {
      modelBreakpoints[i]._scheduleUpdateInDebugger();
      modelBreakpoints[i]._removeEventListeners();
    }

    this._breakpointManager._removeBreakpoint(this, removeFromStorage);
    this._breakpointManager._targetManager.unobserveModels(SDK.DebuggerModel, this);
    this.setPrimaryUISourceCode(null);
  }

  /**
   * @return {string}
   */
  _breakpointStorageId() {
    return Bindings.BreakpointManager._breakpointStorageId(this._url, this._lineNumber, this._columnNumber);
  }

  _resetLocations() {
    this.setPrimaryUISourceCode(null);
    const modelBreakpoints = this._modelBreakpoints.valuesArray();
    for (let i = 0; i < modelBreakpoints.length; ++i)
      modelBreakpoints[i]._resetLocations();
  }
};

/**
 * @unrestricted
 */
Bindings.BreakpointManager.ModelBreakpoint = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   * @param {!Bindings.DebuggerWorkspaceBinding} debuggerWorkspaceBinding
   */
  constructor(debuggerModel, breakpoint, debuggerWorkspaceBinding) {
    this._debuggerModel = debuggerModel;
    this._breakpoint = breakpoint;
    this._debuggerWorkspaceBinding = debuggerWorkspaceBinding;

    this._liveLocations = new Bindings.LiveLocationPool();

    /** @type {!Map<!Bindings.LiveLocation, !Workspace.UILocation>} */
    this._uiLocations = new Map();
    this._debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DebuggerWasDisabled, this._cleanUpAfterDebuggerIsGone, this);
    this._debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DebuggerWasEnabled, this._scheduleUpdateInDebugger, this);
    this._hasPendingUpdate = false;
    this._isUpdating = false;
    this._cancelCallback = false;
    this._currentState = null;
    if (this._debuggerModel.debuggerEnabled())
      this._scheduleUpdateInDebugger();
  }

  _resetLocations() {
    for (const uiLocation of this._uiLocations.values())
      this._breakpoint._uiLocationRemoved(uiLocation);

    this._uiLocations.clear();
    this._liveLocations.disposeAll();
  }

  _scheduleUpdateInDebugger() {
    if (this._isUpdating) {
      this._hasPendingUpdate = true;
      return;
    }

    this._isUpdating = true;
    this._updateInDebugger(this._didUpdateInDebugger.bind(this));
  }

  _didUpdateInDebugger() {
    this._isUpdating = false;
    if (this._hasPendingUpdate) {
      this._hasPendingUpdate = false;
      this._scheduleUpdateInDebugger();
    }
  }

  /**
   * @return {boolean}
   */
  _scriptDiverged() {
    const uiLocation = this._breakpoint._defaultUILocation;
    const uiSourceCode = uiLocation ? uiLocation.uiSourceCode : null;
    if (!uiSourceCode)
      return false;
    const scriptFile = this._debuggerWorkspaceBinding.scriptFile(uiSourceCode, this._debuggerModel);
    return !!scriptFile && scriptFile.hasDivergedFromVM();
  }

  /**
   * @param {function()} callback
   * @return {!Promise}
   */
  async _updateInDebugger(callback) {
    if (this._debuggerModel.target().isDisposed()) {
      this._cleanUpAfterDebuggerIsGone();
      callback();
      return;
    }

    const uiLocation = this._breakpoint._defaultUILocation;
    const uiSourceCode = uiLocation ? uiLocation.uiSourceCode : null;
    const lineNumber = this._breakpoint._lineNumber;
    const columnNumber = this._breakpoint._columnNumber;
    const condition = this._breakpoint.condition();

    let debuggerLocation = null;
    if (uiSourceCode) {
      const locations =
          Bindings.debuggerWorkspaceBinding.uiLocationToRawLocations(uiSourceCode, lineNumber, columnNumber);
      debuggerLocation = locations.find(location => location.debuggerModel === this._debuggerModel);
    }
    let newState;
    if (this._breakpoint._isRemoved || !this._breakpoint.enabled() || this._scriptDiverged()) {
      newState = null;
    } else if (debuggerLocation) {
      const script = debuggerLocation.script();
      if (script.sourceURL) {
        newState = new Bindings.BreakpointManager.Breakpoint.State(
            script.sourceURL, null, null, debuggerLocation.lineNumber, debuggerLocation.columnNumber, condition);
      } else {
        newState = new Bindings.BreakpointManager.Breakpoint.State(
            null, script.scriptId, script.hash, debuggerLocation.lineNumber, debuggerLocation.columnNumber, condition);
      }
    } else if (this._breakpoint._currentState && this._breakpoint._currentState.url) {
      const position = this._breakpoint._currentState;
      newState = new Bindings.BreakpointManager.Breakpoint.State(
          position.url, null, null, position.lineNumber, position.columnNumber, condition);
    } else if (uiSourceCode) {
      newState = new Bindings.BreakpointManager.Breakpoint.State(
          uiSourceCode.url(), null, null, lineNumber, columnNumber, condition);
    }
    if (this._debuggerId && Bindings.BreakpointManager.Breakpoint.State.equals(newState, this._currentState)) {
      callback();
      return;
    }

    this._breakpoint._currentState = newState;

    if (this._debuggerId) {
      await this._refreshBreakpoint();
      callback();
      return;
    }

    if (!newState) {
      callback();
      return;
    }

    let result;
    this._currentState = newState;
    if (newState.url) {
      result = await this._debuggerModel.setBreakpointByURL(
          newState.url, newState.lineNumber, newState.columnNumber, newState.condition);
    } else if (newState.scriptId && newState.scriptHash) {
      result = await this._debuggerModel.setBreakpointInAnonymousScript(
          newState.scriptId, newState.scriptHash, newState.lineNumber, newState.columnNumber, newState.condition);
    }
    if (result && result.breakpointId)
      this._didSetBreakpointInDebugger(callback, result.breakpointId, result.locations);
    else
      this._didSetBreakpointInDebugger(callback, null, []);
  }

  async _refreshBreakpoint() {
    if (!this._debuggerId)
      return;
    this._resetLocations();
    await this._debuggerModel.removeBreakpoint(this._debuggerId);
    this._didRemoveFromDebugger();
    this._currentState = null;
    this._scheduleUpdateInDebugger();
  }

  /**
   * @param {function()} callback
   * @param {?Protocol.Debugger.BreakpointId} breakpointId
   * @param {!Array.<!SDK.DebuggerModel.Location>} locations
   */
  _didSetBreakpointInDebugger(callback, breakpointId, locations) {
    if (this._cancelCallback) {
      this._cancelCallback = false;
      callback();
      return;
    }

    if (!breakpointId) {
      this._breakpoint.remove(true);
      callback();
      return;
    }

    this._debuggerId = breakpointId;
    this._debuggerModel.addBreakpointListener(this._debuggerId, this._breakpointResolved, this);
    for (let i = 0; i < locations.length; ++i) {
      if (!this._addResolvedLocation(locations[i]))
        break;
    }
    callback();
  }

  _didRemoveFromDebugger() {
    if (this._cancelCallback) {
      this._cancelCallback = false;
      return;
    }

    this._resetLocations();
    this._debuggerModel.removeBreakpointListener(this._debuggerId, this._breakpointResolved, this);
    delete this._debuggerId;
  }

  /**
   * @param {!Common.Event} event
   */
  _breakpointResolved(event) {
    this._addResolvedLocation(/** @type {!SDK.DebuggerModel.Location}*/ (event.data));
  }

  /**
   * @param {!Bindings.LiveLocation} liveLocation
   */
  _locationUpdated(liveLocation) {
    const oldUILocation = this._uiLocations.get(liveLocation);
    if (oldUILocation)
      this._breakpoint._uiLocationRemoved(oldUILocation);
    let uiLocation = liveLocation.uiLocation();

    if (uiLocation) {
      const breakpointLocation = this._breakpoint._breakpointManager.findBreakpoint(uiLocation);
      if (breakpointLocation && breakpointLocation.uiLocation !== breakpointLocation.breakpoint._defaultUILocation)
        uiLocation = null;
    }

    if (uiLocation) {
      this._uiLocations.set(liveLocation, uiLocation);
      this._breakpoint._uiLocationAdded(uiLocation);
    } else {
      this._uiLocations.delete(liveLocation);
    }
  }

  /**
   * @param {!SDK.DebuggerModel.Location} location
   * @return {boolean}
   */
  _addResolvedLocation(location) {
    const uiLocation = this._debuggerWorkspaceBinding.rawLocationToUILocation(location);
    if (!uiLocation)
      return false;
    const breakpointLocation = this._breakpoint._breakpointManager.findBreakpoint(uiLocation);
    if (breakpointLocation && breakpointLocation.breakpoint !== this._breakpoint) {
      // location clash
      this._breakpoint.remove(false /* keepInStorage */);
      return false;
    }
    this._debuggerWorkspaceBinding.createLiveLocation(location, this._locationUpdated.bind(this), this._liveLocations);
    return true;
  }

  _cleanUpAfterDebuggerIsGone() {
    if (this._isUpdating)
      this._cancelCallback = true;

    this._resetLocations();
    this._currentState = null;
    if (this._debuggerId)
      this._didRemoveFromDebugger();
  }

  _removeEventListeners() {
    this._debuggerModel.removeEventListener(
        SDK.DebuggerModel.Events.DebuggerWasDisabled, this._cleanUpAfterDebuggerIsGone, this);
    this._debuggerModel.removeEventListener(
        SDK.DebuggerModel.Events.DebuggerWasEnabled, this._scheduleUpdateInDebugger, this);
  }
};

Bindings.BreakpointManager.Breakpoint.State = class {
  /**
   * @param {?string} url
   * @param {?string} scriptId
   * @param {?string} scriptHash
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @param {string} condition
   */
  constructor(url, scriptId, scriptHash, lineNumber, columnNumber, condition) {
    this.url = url;
    this.scriptId = scriptId;
    this.scriptHash = scriptHash;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber;
    this.condition = condition;
  }

  /**
   * @param {?Bindings.BreakpointManager.Breakpoint.State|undefined} stateA
   * @param {?Bindings.BreakpointManager.Breakpoint.State|undefined} stateB
   * @return {boolean}
   */
  static equals(stateA, stateB) {
    if (!stateA || !stateB)
      return false;
    return stateA.url === stateB.url && stateA.scriptId === stateB.scriptId &&
        stateA.scriptHash === stateB.scriptHash && stateA.lineNumber === stateB.lineNumber &&
        stateA.columnNumber === stateB.columnNumber && stateA.condition === stateB.condition;
  }
};


Bindings.BreakpointManager.Storage = class {
  constructor() {
    this._setting = Common.settings.createLocalSetting('breakpoints', []);
    /** @type {!Map<string, !Bindings.BreakpointManager.Storage.Item>} */
    this._breakpoints = new Map();
    const items = /** @type {!Array<!Bindings.BreakpointManager.Storage.Item>} */ (this._setting.get());
    for (const item of items) {
      item.columnNumber = item.columnNumber || 0;
      this._breakpoints.set(
          Bindings.BreakpointManager._breakpointStorageId(item.url, item.lineNumber, item.columnNumber), item);
    }
    /** @type {boolean|undefined} */ this._muted;
  }

  mute() {
    this._muted = true;
  }

  unmute() {
    delete this._muted;
  }

  /**
   * @param {string} url
   * @return {!Array<!Bindings.BreakpointManager.Storage.Item>}
   */
  breakpointItems(url) {
    return Array.from(this._breakpoints.values()).filter(item => item.url === url);
  }

  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   */
  _updateBreakpoint(breakpoint) {
    if (this._muted || !breakpoint._breakpointStorageId())
      return;
    this._breakpoints.set(breakpoint._breakpointStorageId(), new Bindings.BreakpointManager.Storage.Item(breakpoint));
    this._save();
  }

  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   */
  _removeBreakpoint(breakpoint) {
    if (this._muted)
      return;
    this._breakpoints.delete(breakpoint._breakpointStorageId());
    this._save();
  }

  _save() {
    this._setting.set(Array.from(this._breakpoints.values()));
  }
};

/**
 * @unrestricted
 */
Bindings.BreakpointManager.Storage.Item = class {
  /**
   * @param {!Bindings.BreakpointManager.Breakpoint} breakpoint
   */
  constructor(breakpoint) {
    this.url = breakpoint._url;
    this.lineNumber = breakpoint.lineNumber();
    this.columnNumber = breakpoint.columnNumber();
    this.condition = breakpoint.condition();
    this.enabled = breakpoint.enabled();
  }
};

/** @type {!Bindings.BreakpointManager} */
Bindings.breakpointManager;
