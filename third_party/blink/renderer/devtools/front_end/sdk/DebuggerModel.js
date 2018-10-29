/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
SDK.DebuggerModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);

    target.registerDebuggerDispatcher(new SDK.DebuggerDispatcher(this));
    this._agent = target.debuggerAgent();
    this._runtimeModel = /** @type {!SDK.RuntimeModel} */ (target.model(SDK.RuntimeModel));

    /** @type {!SDK.SourceMapManager<!SDK.Script>} */
    this._sourceMapManager = new SDK.SourceMapManager(target);
    /** @type {!Map<string, !SDK.Script>} */
    this._sourceMapIdToScript = new Map();

    /** @type {?SDK.DebuggerPausedDetails} */
    this._debuggerPausedDetails = null;
    /** @type {!Map<string, !SDK.Script>} */
    this._scripts = new Map();
    /** @type {!Map.<string, !Array.<!SDK.Script>>} */
    this._scriptsBySourceURL = new Map();
    /** @type {!Array.<!SDK.Script>} */
    this._discardableScripts = [];

    /** @type {!Common.Object} */
    this._breakpointResolvedEventTarget = new Common.Object();

    this._isPausing = false;
    Common.moduleSetting('pauseOnExceptionEnabled').addChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('pauseOnCaughtException').addChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('disableAsyncStackTraces').addChangeListener(this._asyncStackTracesStateChanged, this);
    Common.moduleSetting('breakpointsActive').addChangeListener(this._breakpointsActiveChanged, this);

    if (!target.suspended())
      this._enableDebugger();

    /** @type {!Map<string, string>} */
    this._stringMap = new Map();
    this._sourceMapManager.setEnabled(Common.moduleSetting('jsSourceMapsEnabled').get());
    Common.moduleSetting('jsSourceMapsEnabled')
        .addChangeListener(event => this._sourceMapManager.setEnabled(/** @type {boolean} */ (event.data)));
  }

  /**
   * @param {string} executionContextId
   * @param {string} sourceURL
   * @param {?string} sourceMapURL
   * @return {?string}
   */
  static _sourceMapId(executionContextId, sourceURL, sourceMapURL) {
    if (!sourceMapURL)
      return null;
    return executionContextId + ':' + sourceURL + ':' + sourceMapURL;
  }

  /**
   * @return {!SDK.SourceMapManager<!SDK.Script>}
   */
  sourceMapManager() {
    return this._sourceMapManager;
  }

  /**
   * @return {!SDK.RuntimeModel}
   */
  runtimeModel() {
    return this._runtimeModel;
  }

  /**
   * @return {boolean}
   */
  debuggerEnabled() {
    return !!this._debuggerEnabled;
  }

  /**
   * @return {!Promise}
   */
  _enableDebugger() {
    if (this._debuggerEnabled)
      return Promise.resolve();
    this._debuggerEnabled = true;

    const enablePromise = this._agent.enable();
    enablePromise.then(this._registerDebugger.bind(this));
    this._pauseOnExceptionStateChanged();
    this._asyncStackTracesStateChanged();
    if (!Common.moduleSetting('breakpointsActive').get())
      this._breakpointsActiveChanged();
    if (SDK.DebuggerModel._scheduledPauseOnAsyncCall)
      this._pauseOnAsyncCall(SDK.DebuggerModel._scheduledPauseOnAsyncCall);
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerWasEnabled, this);
    return enablePromise;
  }

  /**
   * @param {string|null} debuggerId
   */
  _registerDebugger(debuggerId) {
    if (!debuggerId)
      return;
    SDK.DebuggerModel._debuggerIdToModel.set(debuggerId, this);
    this._debuggerId = debuggerId;
  }

  /**
   * @param {string} debuggerId
   * @return {?SDK.DebuggerModel}
   */
  static modelForDebuggerId(debuggerId) {
    return SDK.DebuggerModel._debuggerIdToModel.get(debuggerId) || null;
  }

  /**
   * @return {!Promise}
   */
  _disableDebugger() {
    if (!this._debuggerEnabled)
      return Promise.resolve();
    this._debuggerEnabled = false;

    const disablePromise = this._agent.disable();
    this._isPausing = false;
    this._asyncStackTracesStateChanged();
    this.globalObjectCleared();
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerWasDisabled);
    SDK.DebuggerModel._debuggerIdToModel.delete(this._debuggerId);
    return disablePromise;
  }

  /**
   * @param {boolean} skip
   */
  _skipAllPauses(skip) {
    if (this._skipAllPausesTimeout) {
      clearTimeout(this._skipAllPausesTimeout);
      delete this._skipAllPausesTimeout;
    }
    this._agent.setSkipAllPauses(skip);
  }

  /**
   * @param {number} timeout
   */
  skipAllPausesUntilReloadOrTimeout(timeout) {
    if (this._skipAllPausesTimeout)
      clearTimeout(this._skipAllPausesTimeout);
    this._agent.setSkipAllPauses(true);
    // If reload happens before the timeout, the flag will be already unset and the timeout callback won't change anything.
    this._skipAllPausesTimeout = setTimeout(this._skipAllPauses.bind(this, false), timeout);
  }

  _pauseOnExceptionStateChanged() {
    let state;
    if (!Common.moduleSetting('pauseOnExceptionEnabled').get())
      state = SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions;
    else if (Common.moduleSetting('pauseOnCaughtException').get())
      state = SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions;
    else
      state = SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions;

    this._agent.setPauseOnExceptions(state);
  }

  _asyncStackTracesStateChanged() {
    const maxAsyncStackChainDepth = 32;
    const enabled = !Common.moduleSetting('disableAsyncStackTraces').get() && this._debuggerEnabled;
    this._agent.setAsyncCallStackDepth(enabled ? maxAsyncStackChainDepth : 0);
  }

  _breakpointsActiveChanged() {
    this._agent.setBreakpointsActive(Common.moduleSetting('breakpointsActive').get());
  }

  stepInto() {
    this._agent.stepInto();
  }

  stepOver() {
    this._agent.stepOver();
  }

  stepOut() {
    this._agent.stepOut();
  }

  scheduleStepIntoAsync() {
    this._agent.invoke_stepInto({breakOnAsyncCall: true});
  }

  resume() {
    this._agent.resume();
    this._isPausing = false;
  }

  pause() {
    this._isPausing = true;
    this._skipAllPauses(false);
    this._agent.pause();
  }

  /**
   * @param {!Protocol.Runtime.StackTraceId} parentStackTraceId
   * @return {!Promise}
   */
  _pauseOnAsyncCall(parentStackTraceId) {
    return this._agent.invoke_pauseOnAsyncCall({parentStackTraceId: parentStackTraceId});
  }

  /**
   * @param {string} url
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {string=} condition
   * @return {!Promise<!SDK.DebuggerModel.SetBreakpointResult>}
   */
  async setBreakpointByURL(url, lineNumber, columnNumber, condition) {
    // Convert file url to node-js path.
    let urlRegex;
    if (this.target().isNodeJS()) {
      const platformPath = Common.ParsedURL.urlToPlatformPath(url, Host.isWin());
      urlRegex = `${platformPath.escapeForRegExp()}|${url.escapeForRegExp()}`;
    }
    // Adjust column if needed.
    let minColumnNumber = 0;
    const scripts = this._scriptsBySourceURL.get(url) || [];
    for (let i = 0, l = scripts.length; i < l; ++i) {
      const script = scripts[i];
      if (lineNumber === script.lineOffset)
        minColumnNumber = minColumnNumber ? Math.min(minColumnNumber, script.columnOffset) : script.columnOffset;
    }
    columnNumber = Math.max(columnNumber, minColumnNumber);
    const response = await this._agent.invoke_setBreakpointByUrl({
      lineNumber: lineNumber,
      url: urlRegex ? undefined : url,
      urlRegex: urlRegex,
      columnNumber: columnNumber,
      condition: condition
    });
    if (response[Protocol.Error])
      return {locations: [], breakpointId: null};
    let locations;
    if (response.locations)
      locations = response.locations.map(payload => SDK.DebuggerModel.Location.fromPayload(this, payload));
    return {locations: locations, breakpointId: response.breakpointId};
  }

  /**
   * @param {string} scriptId
   * @param {string} scriptHash
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {string=} condition
   * @return {!Promise<!SDK.DebuggerModel.SetBreakpointResult>}
   */
  async setBreakpointInAnonymousScript(scriptId, scriptHash, lineNumber, columnNumber, condition) {
    const response = await this._agent.invoke_setBreakpointByUrl(
        {lineNumber: lineNumber, scriptHash: scriptHash, columnNumber: columnNumber, condition: condition});
    const error = response[Protocol.Error];
    if (error) {
      // Old V8 backend doesn't support scriptHash argument.
      if (error !== 'Either url or urlRegex must be specified.')
        return {locations: [], breakpointId: null};
      return this._setBreakpointBySourceId(scriptId, lineNumber, columnNumber, condition);
    }
    let locations;
    if (response.locations)
      locations = response.locations.map(payload => SDK.DebuggerModel.Location.fromPayload(this, payload));
    return {locations: locations, breakpointId: response.breakpointId};
  }

  /**
   * @param {string} scriptId
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {string=} condition
   * @return {!Promise<!SDK.DebuggerModel.SetBreakpointResult>}
   */
  async _setBreakpointBySourceId(scriptId, lineNumber, columnNumber, condition) {
    // This method is required for backward compatibility with V8 before 6.3.275.
    const response = await this._agent.invoke_setBreakpoint(
        {location: {scriptId: scriptId, lineNumber: lineNumber, columnNumber: columnNumber}, condition: condition});
    if (response[Protocol.Error])
      return {breakpointId: null, locations: []};
    let actualLocation = [];
    if (response.actualLocation)
      actualLocation = [SDK.DebuggerModel.Location.fromPayload(this, response.actualLocation)];
    return {locations: actualLocation, breakpointId: response.breakpointId};
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @return {!Promise}
   */
  async removeBreakpoint(breakpointId) {
    const response = await this._agent.invoke_removeBreakpoint({breakpointId});
    if (response[Protocol.Error])
      console.error('Failed to remove breakpoint: ' + response[Protocol.Error]);
  }

  /**
   * @param {!SDK.DebuggerModel.Location} startLocation
   * @param {?SDK.DebuggerModel.Location} endLocation
   * @param {boolean} restrictToFunction
   * @return {!Promise<!Array<!SDK.DebuggerModel.BreakLocation>>}
   */
  async getPossibleBreakpoints(startLocation, endLocation, restrictToFunction) {
    const response = await this._agent.invoke_getPossibleBreakpoints({
      start: startLocation.payload(),
      end: endLocation ? endLocation.payload() : undefined,
      restrictToFunction: restrictToFunction
    });
    if (response[Protocol.Error] || !response.locations)
      return [];
    return response.locations.map(location => SDK.DebuggerModel.BreakLocation.fromPayload(this, location));
  }

  /**
   * @param {!Protocol.Runtime.StackTraceId} stackId
   * @return {!Promise<?Protocol.Runtime.StackTrace>}
   */
  async fetchAsyncStackTrace(stackId) {
    const response = await this._agent.invoke_getStackTrace({stackTraceId: stackId});
    return response[Protocol.Error] ? null : response.stackTrace;
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {!Protocol.Debugger.Location} location
   */
  _breakpointResolved(breakpointId, location) {
    this._breakpointResolvedEventTarget.dispatchEventToListeners(
        breakpointId, SDK.DebuggerModel.Location.fromPayload(this, location));
  }

  globalObjectCleared() {
    this._setDebuggerPausedDetails(null);
    this._reset();
    // TODO(dgozman): move clients to ExecutionContextDestroyed/ScriptCollected events.
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.GlobalObjectCleared, this);
  }

  _reset() {
    for (const scriptWithSourceMap of this._sourceMapIdToScript.values())
      this._sourceMapManager.detachSourceMap(scriptWithSourceMap);
    this._sourceMapIdToScript.clear();

    this._scripts.clear();
    this._scriptsBySourceURL.clear();
    this._stringMap.clear();
    this._discardableScripts = [];
  }

  /**
   * @return {!Array<!SDK.Script>}
   */
  scripts() {
    return Array.from(this._scripts.values());
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @return {?SDK.Script}
   */
  scriptForId(scriptId) {
    return this._scripts.get(scriptId) || null;
  }

  /**
   * @return {!Array.<!SDK.Script>}
   */
  scriptsForSourceURL(sourceURL) {
    if (!sourceURL)
      return [];
    return this._scriptsBySourceURL.get(sourceURL) || [];
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {!Array<!SDK.Script>}
   */
  scriptsForExecutionContext(executionContext) {
    const result = [];
    for (const script of this._scripts.values()) {
      if (script.executionContextId === executionContext.id)
        result.push(script);
    }
    return result;
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} newSource
   * @param {function(?Protocol.Error, !Protocol.Runtime.ExceptionDetails=)} callback
   */
  setScriptSource(scriptId, newSource, callback) {
    this._scripts.get(scriptId).editSource(
        newSource, this._didEditScriptSource.bind(this, scriptId, newSource, callback));
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} newSource
   * @param {function(?Protocol.Error, !Protocol.Runtime.ExceptionDetails=)} callback
   * @param {?Protocol.Error} error
   * @param {!Protocol.Runtime.ExceptionDetails=} exceptionDetails
   * @param {!Array.<!Protocol.Debugger.CallFrame>=} callFrames
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   * @param {!Protocol.Runtime.StackTraceId=} asyncStackTraceId
   * @param {boolean=} needsStepIn
   */
  _didEditScriptSource(
      scriptId, newSource, callback, error, exceptionDetails, callFrames, asyncStackTrace, asyncStackTraceId,
      needsStepIn) {
    callback(error, exceptionDetails);
    if (needsStepIn) {
      this.stepInto();
      return;
    }

    if (!error && callFrames && callFrames.length) {
      this._pausedScript(
          callFrames, this._debuggerPausedDetails.reason, this._debuggerPausedDetails.auxData,
          this._debuggerPausedDetails.breakpointIds, asyncStackTrace, asyncStackTraceId);
    }
  }

  /**
   * @return {?Array.<!SDK.DebuggerModel.CallFrame>}
   */
  get callFrames() {
    return this._debuggerPausedDetails ? this._debuggerPausedDetails.callFrames : null;
  }

  /**
   * @return {?SDK.DebuggerPausedDetails}
   */
  debuggerPausedDetails() {
    return this._debuggerPausedDetails;
  }

  /**
   * @param {?SDK.DebuggerPausedDetails} debuggerPausedDetails
   * @return {boolean}
   */
  _setDebuggerPausedDetails(debuggerPausedDetails) {
    this._isPausing = false;
    this._debuggerPausedDetails = debuggerPausedDetails;
    if (this._debuggerPausedDetails) {
      if (Runtime.experiments.isEnabled('emptySourceMapAutoStepping') && this._beforePausedCallback) {
        if (!this._beforePausedCallback.call(null, this._debuggerPausedDetails))
          return false;
      }
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerPaused, this);
    }
    if (debuggerPausedDetails)
      this.setSelectedCallFrame(debuggerPausedDetails.callFrames[0]);
    else
      this.setSelectedCallFrame(null);
    return true;
  }

  /**
   * @param {?function(!SDK.DebuggerPausedDetails):boolean} callback
   */
  setBeforePausedCallback(callback) {
    this._beforePausedCallback = callback;
  }

  /**
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object|undefined} auxData
   * @param {!Array.<string>} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   * @param {!Protocol.Runtime.StackTraceId=} asyncStackTraceId
   * @param {!Protocol.Runtime.StackTraceId=} asyncCallStackTraceId
   */
  async _pausedScript(
      callFrames, reason, auxData, breakpointIds, asyncStackTrace, asyncStackTraceId, asyncCallStackTraceId) {
    if (asyncCallStackTraceId) {
      SDK.DebuggerModel._scheduledPauseOnAsyncCall = asyncCallStackTraceId;
      const promises = [];
      for (const model of SDK.DebuggerModel._debuggerIdToModel.values())
        promises.push(model._pauseOnAsyncCall(asyncCallStackTraceId));
      await Promise.all(promises);
      this.resume();
      return;
    }

    const pausedDetails = new SDK.DebuggerPausedDetails(
        this, callFrames, reason, auxData, breakpointIds, asyncStackTrace, asyncStackTraceId);

    if (pausedDetails && this._continueToLocationCallback) {
      const callback = this._continueToLocationCallback;
      delete this._continueToLocationCallback;
      if (callback(pausedDetails))
        return;
    }

    if (!this._setDebuggerPausedDetails(pausedDetails))
      this._agent.stepInto();

    SDK.DebuggerModel._scheduledPauseOnAsyncCall = null;
  }

  _resumedScript() {
    this._setDebuggerPausedDetails(null);
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerResumed, this);
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*|undefined} executionContextAuxData
   * @param {boolean} isLiveEdit
   * @param {string|undefined} sourceMapURL
   * @param {boolean} hasSourceURLComment
   * @param {boolean} hasSyntaxError
   * @param {number} length
   * @param {?Protocol.Runtime.StackTrace} originStackTrace
   * @return {!SDK.Script}
   */
  _parsedScriptSource(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, isLiveEdit, sourceMapURL, hasSourceURLComment, hasSyntaxError, length,
      originStackTrace) {
    if (this._scripts.has(scriptId))
      return this._scripts.get(scriptId);
    let isContentScript = false;
    if (executionContextAuxData && ('isDefault' in executionContextAuxData))
      isContentScript = !executionContextAuxData['isDefault'];
    sourceURL = this._internString(sourceURL);
    const script = new SDK.Script(
        this, scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId,
        this._internString(hash), isContentScript, isLiveEdit, sourceMapURL, hasSourceURLComment, length,
        originStackTrace);
    this._registerScript(script);
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.ParsedScriptSource, script);

    const sourceMapId =
        SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (sourceMapId && !hasSyntaxError) {
      // Consecutive script evaluations in the same execution context with the same sourceURL
      // and sourceMappingURL should result in source map reloading.
      const previousScript = this._sourceMapIdToScript.get(sourceMapId);
      if (previousScript)
        this._sourceMapManager.detachSourceMap(previousScript);
      this._sourceMapIdToScript.set(sourceMapId, script);
      this._sourceMapManager.attachSourceMap(script, script.sourceURL, script.sourceMapURL);
    }

    const isDiscardable = hasSyntaxError && script.isAnonymousScript();
    if (isDiscardable) {
      this._discardableScripts.push(script);
      this._collectDiscardedScripts();
    }
    return script;
  }

  /**
   * @param {!SDK.Script} script
   * @param {string} newSourceMapURL
   */
  setSourceMapURL(script, newSourceMapURL) {
    let sourceMapId = SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (sourceMapId && this._sourceMapIdToScript.get(sourceMapId) === script)
      this._sourceMapIdToScript.delete(sourceMapId);
    this._sourceMapManager.detachSourceMap(script);

    script.sourceMapURL = newSourceMapURL;
    sourceMapId = SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (!sourceMapId)
      return;
    this._sourceMapIdToScript.set(sourceMapId, script);
    this._sourceMapManager.attachSourceMap(script, script.sourceURL, script.sourceMapURL);
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   */
  executionContextDestroyed(executionContext) {
    const sourceMapIds = Array.from(this._sourceMapIdToScript.keys());
    for (const sourceMapId of sourceMapIds) {
      const script = this._sourceMapIdToScript.get(sourceMapId);
      if (script.executionContextId === executionContext.id) {
        this._sourceMapIdToScript.delete(sourceMapId);
        this._sourceMapManager.detachSourceMap(script);
      }
    }
  }

  /**
   * @param {!SDK.Script} script
   */
  _registerScript(script) {
    this._scripts.set(script.scriptId, script);
    if (script.isAnonymousScript())
      return;

    let scripts = this._scriptsBySourceURL.get(script.sourceURL);
    if (!scripts) {
      scripts = [];
      this._scriptsBySourceURL.set(script.sourceURL, scripts);
    }
    scripts.push(script);
  }

  /**
   * @param {!SDK.Script} script
   */
  _unregisterScript(script) {
    console.assert(script.isAnonymousScript());
    this._scripts.delete(script.scriptId);
  }

  _collectDiscardedScripts() {
    if (this._discardableScripts.length < 1000)
      return;
    const scriptsToDiscard = this._discardableScripts.splice(0, 100);
    for (const script of scriptsToDiscard) {
      this._unregisterScript(script);
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.DiscardedAnonymousScriptSource, script);
    }
  }

  /**
   * @param {!SDK.Script} script
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocation(script, lineNumber, columnNumber) {
    return new SDK.DebuggerModel.Location(this, script.scriptId, lineNumber, columnNumber);
  }

  /**
   * @param {string} sourceURL
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocationByURL(sourceURL, lineNumber, columnNumber) {
    let closestScript = null;
    const scripts = this._scriptsBySourceURL.get(sourceURL) || [];
    for (let i = 0, l = scripts.length; i < l; ++i) {
      const script = scripts[i];
      if (!closestScript)
        closestScript = script;
      if (script.lineOffset > lineNumber || (script.lineOffset === lineNumber && script.columnOffset > columnNumber))
        continue;
      if (script.endLine < lineNumber || (script.endLine === lineNumber && script.endColumn <= columnNumber))
        continue;
      closestScript = script;
      break;
    }
    return closestScript ? new SDK.DebuggerModel.Location(this, closestScript.scriptId, lineNumber, columnNumber) :
                           null;
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocationByScriptId(scriptId, lineNumber, columnNumber) {
    const script = this.scriptForId(scriptId);
    return script ? this.createRawLocation(script, lineNumber, columnNumber) : null;
  }

  /**
   * @param {!Protocol.Runtime.StackTrace} stackTrace
   * @return {!Array<!SDK.DebuggerModel.Location>}
   */
  createRawLocationsByStackTrace(stackTrace) {
    const frames = [];
    while (stackTrace) {
      for (const frame of stackTrace.callFrames)
        frames.push(frame);
      stackTrace = stackTrace.parent;
    }

    const rawLocations = [];
    for (const frame of frames) {
      const rawLocation = this.createRawLocationByScriptId(frame.scriptId, frame.lineNumber, frame.columnNumber);
      if (rawLocation)
        rawLocations.push(rawLocation);
    }
    return rawLocations;
  }

  /**
   * @return {boolean}
   */
  isPaused() {
    return !!this.debuggerPausedDetails();
  }

  /**
   * @return {boolean}
   */
  isPausing() {
    return this._isPausing;
  }

  /**
   * @param {?SDK.DebuggerModel.CallFrame} callFrame
   */
  setSelectedCallFrame(callFrame) {
    if (this._selectedCallFrame === callFrame)
      return;
    this._selectedCallFrame = callFrame;
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.CallFrameSelected, this);
  }

  /**
   * @return {?SDK.DebuggerModel.CallFrame}
   */
  selectedCallFrame() {
    return this._selectedCallFrame;
  }

  /**
   * @param {!SDK.RuntimeModel.EvaluationOptions} options
   * @return {!Promise<!SDK.RuntimeModel.EvaluationResult>}
   */
  evaluateOnSelectedCallFrame(options) {
    return this.selectedCallFrame().evaluate(options);
  }

  /**
   * @param {!SDK.RemoteObject} remoteObject
   * @return {!Promise<?SDK.DebuggerModel.FunctionDetails>}
   */
  functionDetailsPromise(remoteObject) {
    return remoteObject.getAllProperties(false /* accessorPropertiesOnly */, false /* generatePreview */)
        .then(buildDetails.bind(this));

    /**
     * @param {!SDK.GetPropertiesResult} response
     * @return {?SDK.DebuggerModel.FunctionDetails}
     * @this {!SDK.DebuggerModel}
     */
    function buildDetails(response) {
      if (!response)
        return null;
      let location = null;
      if (response.internalProperties) {
        for (const prop of response.internalProperties) {
          if (prop.name === '[[FunctionLocation]]')
            location = prop.value;
        }
      }
      let functionName = null;
      if (response.properties) {
        for (const prop of response.properties) {
          if (prop.name === 'name' && prop.value && prop.value.type === 'string')
            functionName = prop.value;
          if (prop.name === 'displayName' && prop.value && prop.value.type === 'string') {
            functionName = prop.value;
            break;
          }
        }
      }
      let debuggerLocation = null;
      if (location) {
        debuggerLocation = this.createRawLocationByScriptId(
            location.value.scriptId, location.value.lineNumber, location.value.columnNumber);
      }
      return {location: debuggerLocation, functionName: functionName ? functionName.value : ''};
    }
  }

  /**
   * @param {number} scopeNumber
   * @param {string} variableName
   * @param {!Protocol.Runtime.CallArgument} newValue
   * @param {string} callFrameId
   * @return {!Promise<string|undefined>}
   */
  async setVariableValue(scopeNumber, variableName, newValue, callFrameId) {
    const response = await this._agent.invoke_setVariableValue({scopeNumber, variableName, newValue, callFrameId});
    const error = response[Protocol.Error];
    if (error)
      console.error(error);
    return error;
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {function(!Common.Event)} listener
   * @param {!Object=} thisObject
   */
  addBreakpointListener(breakpointId, listener, thisObject) {
    this._breakpointResolvedEventTarget.addEventListener(breakpointId, listener, thisObject);
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {function(!Common.Event)} listener
   * @param {!Object=} thisObject
   */
  removeBreakpointListener(breakpointId, listener, thisObject) {
    this._breakpointResolvedEventTarget.removeEventListener(breakpointId, listener, thisObject);
  }

  /**
   * @param {!Array<string>} patterns
   * @return {!Promise<boolean>}
   */
  async setBlackboxPatterns(patterns) {
    const response = await this._agent.invoke_setBlackboxPatterns({patterns});
    const error = response[Protocol.Error];
    if (error)
      console.error(error);
    return !error;
  }

  /**
   * @override
   */
  dispose() {
    this._sourceMapManager.dispose();
    SDK.DebuggerModel._debuggerIdToModel.delete(this._debuggerId);
    Common.moduleSetting('pauseOnExceptionEnabled').removeChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('pauseOnCaughtException').removeChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('disableAsyncStackTraces').removeChangeListener(this._asyncStackTracesStateChanged, this);
  }

  /**
   * @override
   * @return {!Promise}
   */
  async suspendModel() {
    await this._disableDebugger();
  }

  /**
   * @override
   * @return {!Promise}
   */
  async resumeModel() {
    await this._enableDebugger();
  }

  /**
   * @param {string} string
   * @return {string} string
   */
  _internString(string) {
    if (!this._stringMap.has(string))
      this._stringMap.set(string, string);
    return this._stringMap.get(string);
  }
};

/** @type {!Map<string, !SDK.DebuggerModel>} */
SDK.DebuggerModel._debuggerIdToModel = new Map();

/** @type {?Protocol.Runtime.StackTraceId} */
SDK.DebuggerModel._scheduledPauseOnAsyncCall = null;

SDK.SDKModel.register(SDK.DebuggerModel, SDK.Target.Capability.JS, true);

/** @typedef {{location: ?SDK.DebuggerModel.Location, functionName: string}} */
SDK.DebuggerModel.FunctionDetails;

/**
 * Keep these in sync with WebCore::V8Debugger
 *
 * @enum {string}
 */
SDK.DebuggerModel.PauseOnExceptionsState = {
  DontPauseOnExceptions: 'none',
  PauseOnAllExceptions: 'all',
  PauseOnUncaughtExceptions: 'uncaught'
};

/** @enum {symbol} */
SDK.DebuggerModel.Events = {
  DebuggerWasEnabled: Symbol('DebuggerWasEnabled'),
  DebuggerWasDisabled: Symbol('DebuggerWasDisabled'),
  DebuggerPaused: Symbol('DebuggerPaused'),
  DebuggerResumed: Symbol('DebuggerResumed'),
  ParsedScriptSource: Symbol('ParsedScriptSource'),
  FailedToParseScriptSource: Symbol('FailedToParseScriptSource'),
  DiscardedAnonymousScriptSource: Symbol('DiscardedAnonymousScriptSource'),
  GlobalObjectCleared: Symbol('GlobalObjectCleared'),
  CallFrameSelected: Symbol('CallFrameSelected'),
  ConsoleCommandEvaluatedInSelectedCallFrame: Symbol('ConsoleCommandEvaluatedInSelectedCallFrame')
};

/** @enum {string} */
SDK.DebuggerModel.BreakReason = {
  DOM: 'DOM',
  EventListener: 'EventListener',
  XHR: 'XHR',
  Exception: 'exception',
  PromiseRejection: 'promiseRejection',
  Assert: 'assert',
  DebugCommand: 'debugCommand',
  OOM: 'OOM',
  Other: 'other'
};

/** @enum {string} */
SDK.DebuggerModel.BreakLocationType = {
  Return: 'return',
  Call: 'call',
  DebuggerStatement: 'debuggerStatement'
};

SDK.DebuggerEventTypes = {
  JavaScriptPause: 0,
  JavaScriptBreakpoint: 1,
  NativeBreakpoint: 2
};

SDK.DebuggerModel.ContinueToLocationTargetCallFrames = {
  Any: 'any',
  Current: 'current'
};

/** @typedef {{
 *    breakpointId: ?Protocol.Debugger.BreakpointId,
 *    locations: !Array<!SDK.DebuggerModel.Location>
 *  }}
 */
SDK.DebuggerModel.SetBreakpointResult;

/**
 * @extends {Protocol.DebuggerDispatcher}
 * @unrestricted
 */
SDK.DebuggerDispatcher = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  constructor(debuggerModel) {
    this._debuggerModel = debuggerModel;
  }

  /**
   * @override
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object=} auxData
   * @param {!Array.<string>=} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   * @param {!Protocol.Runtime.StackTraceId=} asyncStackTraceId
   * @param {!Protocol.Runtime.StackTraceId=} asyncCallStackTraceId
   */
  paused(callFrames, reason, auxData, breakpointIds, asyncStackTrace, asyncStackTraceId, asyncCallStackTraceId) {
    this._debuggerModel._pausedScript(
        callFrames, reason, auxData, breakpointIds || [], asyncStackTrace, asyncStackTraceId, asyncCallStackTraceId);
  }

  /**
   * @override
   */
  resumed() {
    this._debuggerModel._resumedScript();
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*=} executionContextAuxData
   * @param {boolean=} isLiveEdit
   * @param {string=} sourceMapURL
   * @param {boolean=} hasSourceURL
   * @param {boolean=} isModule
   * @param {number=} length
   * @param {!Protocol.Runtime.StackTrace=} stackTrace
   */
  scriptParsed(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, isLiveEdit, sourceMapURL, hasSourceURL, isModule, length, stackTrace) {
    this._debuggerModel._parsedScriptSource(
        scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
        executionContextAuxData, !!isLiveEdit, sourceMapURL, !!hasSourceURL, false, length || 0, stackTrace || null);
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*=} executionContextAuxData
   * @param {string=} sourceMapURL
   * @param {boolean=} hasSourceURL
   * @param {boolean=} isModule
   * @param {number=} length
   * @param {!Protocol.Runtime.StackTrace=} stackTrace
   */
  scriptFailedToParse(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, sourceMapURL, hasSourceURL, isModule, length, stackTrace) {
    this._debuggerModel._parsedScriptSource(
        scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
        executionContextAuxData, false, sourceMapURL, !!hasSourceURL, true, length || 0, stackTrace || null);
  }

  /**
   * @override
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {!Protocol.Debugger.Location} location
   */
  breakpointResolved(breakpointId, location) {
    this._debuggerModel._breakpointResolved(breakpointId, location);
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.Location = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {string} scriptId
   * @param {number} lineNumber
   * @param {number=} columnNumber
   */
  constructor(debuggerModel, scriptId, lineNumber, columnNumber) {
    this.debuggerModel = debuggerModel;
    this.scriptId = scriptId;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber || 0;
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Protocol.Debugger.Location} payload
   * @return {!SDK.DebuggerModel.Location}
   */
  static fromPayload(debuggerModel, payload) {
    return new SDK.DebuggerModel.Location(debuggerModel, payload.scriptId, payload.lineNumber, payload.columnNumber);
  }

  /**
   * @return {!Protocol.Debugger.Location}
   */
  payload() {
    return {scriptId: this.scriptId, lineNumber: this.lineNumber, columnNumber: this.columnNumber};
  }

  /**
   * @return {?SDK.Script}
   */
  script() {
    return this.debuggerModel.scriptForId(this.scriptId);
  }

  /**
   * @param {function()=} pausedCallback
   */
  continueToLocation(pausedCallback) {
    if (pausedCallback)
      this.debuggerModel._continueToLocationCallback = this._paused.bind(this, pausedCallback);
    this.debuggerModel._agent.continueToLocation(
        this.payload(), SDK.DebuggerModel.ContinueToLocationTargetCallFrames.Current);
  }

  /**
   * @param {function()|undefined} pausedCallback
   * @param {!SDK.DebuggerPausedDetails} debuggerPausedDetails
   * @return {boolean}
   */
  _paused(pausedCallback, debuggerPausedDetails) {
    const location = debuggerPausedDetails.callFrames[0].location();
    if (location.scriptId === this.scriptId && location.lineNumber === this.lineNumber &&
        location.columnNumber === this.columnNumber) {
      pausedCallback();
      return true;
    }
    return false;
  }

  /**
   * @return {string}
   */
  id() {
    return this.debuggerModel.target().id() + ':' + this.scriptId + ':' + this.lineNumber + ':' + this.columnNumber;
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.BreakLocation = class extends SDK.DebuggerModel.Location {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {string} scriptId
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {!Protocol.Debugger.BreakLocationType=} type
   */
  constructor(debuggerModel, scriptId, lineNumber, columnNumber, type) {
    super(debuggerModel, scriptId, lineNumber, columnNumber);
    if (type)
      this.type = type;
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Protocol.Debugger.BreakLocation} payload
   * @return {!SDK.DebuggerModel.BreakLocation}
   */
  static fromPayload(debuggerModel, payload) {
    return new SDK.DebuggerModel.BreakLocation(
        debuggerModel, payload.scriptId, payload.lineNumber, payload.columnNumber, payload.type);
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.CallFrame = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!SDK.Script} script
   * @param {!Protocol.Debugger.CallFrame} payload
   */
  constructor(debuggerModel, script, payload) {
    this.debuggerModel = debuggerModel;
    this._script = script;
    this._payload = payload;
    this._location = SDK.DebuggerModel.Location.fromPayload(debuggerModel, payload.location);
    this._scopeChain = [];
    this._localScope = null;
    for (let i = 0; i < payload.scopeChain.length; ++i) {
      const scope = new SDK.DebuggerModel.Scope(this, i);
      this._scopeChain.push(scope);
      if (scope.type() === Protocol.Debugger.ScopeType.Local)
        this._localScope = scope;
    }
    if (payload.functionLocation)
      this._functionLocation = SDK.DebuggerModel.Location.fromPayload(debuggerModel, payload.functionLocation);
    this._returnValue =
        payload.returnValue ? this.debuggerModel._runtimeModel.createRemoteObject(payload.returnValue) : null;
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @return {!Array.<!SDK.DebuggerModel.CallFrame>}
   */
  static fromPayloadArray(debuggerModel, callFrames) {
    const result = [];
    for (let i = 0; i < callFrames.length; ++i) {
      const callFrame = callFrames[i];
      const script = debuggerModel.scriptForId(callFrame.location.scriptId);
      if (script)
        result.push(new SDK.DebuggerModel.CallFrame(debuggerModel, script, callFrame));
    }
    return result;
  }

  /**
   * @return {!SDK.Script}
   */
  get script() {
    return this._script;
  }

  /**
   * @return {string}
   */
  get id() {
    return this._payload.callFrameId;
  }

  /**
   * @return {!Array.<!SDK.DebuggerModel.Scope>}
   */
  scopeChain() {
    return this._scopeChain;
  }

  /**
   * @return {?SDK.DebuggerModel.Scope}
   */
  localScope() {
    return this._localScope;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  thisObject() {
    return this._payload.this ? this.debuggerModel._runtimeModel.createRemoteObject(this._payload.this) : null;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  returnValue() {
    return this._returnValue;
  }

  /**
   * @param {string} expression
   * @return {!Promise<?SDK.RemoteObject>}
   */
  async setReturnValue(expression) {
    if (!this._returnValue)
      return null;

    const evaluateResponse = await this.debuggerModel._agent.invoke_evaluateOnCallFrame(
        {callFrameId: this.id, expression: expression, silent: true, objectGroup: 'backtrace'});
    if (evaluateResponse[Protocol.Error] || evaluateResponse.exceptionDetails)
      return null;
    const response = await this.debuggerModel._agent.invoke_setReturnValue({newValue: evaluateResponse.result});
    if (response[Protocol.Error])
      return null;
    this._returnValue = this.debuggerModel._runtimeModel.createRemoteObject(evaluateResponse.result);
    return this._returnValue;
  }

  /**
   * @return {string}
   */
  get functionName() {
    return this._payload.functionName;
  }

  /**
   * @return {!SDK.DebuggerModel.Location}
   */
  location() {
    return this._location;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  functionLocation() {
    return this._functionLocation || null;
  }

  /**
   * @param {!SDK.RuntimeModel.EvaluationOptions} options
   * @return {!Promise<!SDK.RuntimeModel.EvaluationResult>}
   */
  async evaluate(options) {
    const runtimeModel = this.debuggerModel.runtimeModel();
    // Assume backends either support both throwOnSideEffect and timeout options or neither.
    const needsTerminationOptions = !!options.throwOnSideEffect || options.timeout !== undefined;
    if (needsTerminationOptions &&
        (runtimeModel.hasSideEffectSupport() === false ||
         (runtimeModel.hasSideEffectSupport() === null && !await runtimeModel.checkSideEffectSupport())))
      return {error: 'Side-effect checks not supported by backend.'};

    const response = await this.debuggerModel._agent.invoke_evaluateOnCallFrame({
      callFrameId: this.id,
      expression: options.expression,
      objectGroup: options.objectGroup,
      includeCommandLineAPI: options.includeCommandLineAPI,
      silent: options.silent,
      returnByValue: options.returnByValue,
      generatePreview: options.generatePreview,
      throwOnSideEffect: options.throwOnSideEffect,
      timeout: options.timeout
    });
    const error = response[Protocol.Error];
    if (error) {
      console.error(error);
      return {error: error};
    }
    return {object: runtimeModel.createRemoteObject(response.result), exceptionDetails: response.exceptionDetails};
  }

  async restart() {
    const response = await this.debuggerModel._agent.invoke_restartFrame({callFrameId: this._payload.callFrameId});
    if (!response[Protocol.Error])
      this.debuggerModel.stepInto();
  }
};


/**
 * @unrestricted
 */
SDK.DebuggerModel.Scope = class {
  /**
   * @param {!SDK.DebuggerModel.CallFrame} callFrame
   * @param {number} ordinal
   */
  constructor(callFrame, ordinal) {
    this._callFrame = callFrame;
    this._payload = callFrame._payload.scopeChain[ordinal];
    this._type = this._payload.type;
    this._name = this._payload.name;
    this._ordinal = ordinal;
    this._startLocation = this._payload.startLocation ?
        SDK.DebuggerModel.Location.fromPayload(callFrame.debuggerModel, this._payload.startLocation) :
        null;
    this._endLocation = this._payload.endLocation ?
        SDK.DebuggerModel.Location.fromPayload(callFrame.debuggerModel, this._payload.endLocation) :
        null;
  }

  /**
   * @return {!SDK.DebuggerModel.CallFrame}
   */
  callFrame() {
    return this._callFrame;
  }

  /**
   * @return {string}
   */
  type() {
    return this._type;
  }

  /**
   * @return {string}
   */
  typeName() {
    switch (this._type) {
      case Protocol.Debugger.ScopeType.Local:
        return Common.UIString('Local');
      case Protocol.Debugger.ScopeType.Closure:
        return Common.UIString('Closure');
      case Protocol.Debugger.ScopeType.Catch:
        return Common.UIString('Catch');
      case Protocol.Debugger.ScopeType.Block:
        return Common.UIString('Block');
      case Protocol.Debugger.ScopeType.Script:
        return Common.UIString('Script');
      case Protocol.Debugger.ScopeType.With:
        return Common.UIString('With Block');
      case Protocol.Debugger.ScopeType.Global:
        return Common.UIString('Global');
      case Protocol.Debugger.ScopeType.Module:
        return Common.UIString('Module');
    }
    return '';
  }


  /**
   * @return {string|undefined}
   */
  name() {
    return this._name;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  startLocation() {
    return this._startLocation;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  endLocation() {
    return this._endLocation;
  }

  /**
   * @return {!SDK.RemoteObject}
   */
  object() {
    if (this._object)
      return this._object;
    const runtimeModel = this._callFrame.debuggerModel._runtimeModel;

    const declarativeScope =
        this._type !== Protocol.Debugger.ScopeType.With && this._type !== Protocol.Debugger.ScopeType.Global;
    if (declarativeScope) {
      this._object = runtimeModel.createScopeRemoteObject(
          this._payload.object, new SDK.ScopeRef(this._ordinal, this._callFrame.id));
    } else {
      this._object = runtimeModel.createRemoteObject(this._payload.object);
    }

    return this._object;
  }

  /**
   * @return {string}
   */
  description() {
    const declarativeScope =
        this._type !== Protocol.Debugger.ScopeType.With && this._type !== Protocol.Debugger.ScopeType.Global;
    return declarativeScope ? '' : (this._payload.object.description || '');
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerPausedDetails = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object|undefined} auxData
   * @param {!Array.<string>} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   * @param {!Protocol.Runtime.StackTraceId=} asyncStackTraceId
   */
  constructor(debuggerModel, callFrames, reason, auxData, breakpointIds, asyncStackTrace, asyncStackTraceId) {
    this.debuggerModel = debuggerModel;
    this.callFrames = SDK.DebuggerModel.CallFrame.fromPayloadArray(debuggerModel, callFrames);
    this.reason = reason;
    this.auxData = auxData;
    this.breakpointIds = breakpointIds;
    if (asyncStackTrace)
      this.asyncStackTrace = this._cleanRedundantFrames(asyncStackTrace);
    this.asyncStackTraceId = asyncStackTraceId;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  exception() {
    if (this.reason !== SDK.DebuggerModel.BreakReason.Exception &&
        this.reason !== SDK.DebuggerModel.BreakReason.PromiseRejection)
      return null;
    return this.debuggerModel._runtimeModel.createRemoteObject(
        /** @type {!Protocol.Runtime.RemoteObject} */ (this.auxData));
  }

  /**
   * @param {!Protocol.Runtime.StackTrace} asyncStackTrace
   * @return {!Protocol.Runtime.StackTrace}
   */
  _cleanRedundantFrames(asyncStackTrace) {
    let stack = asyncStackTrace;
    let previous = null;
    while (stack) {
      if (stack.description === 'async function' && stack.callFrames.length)
        stack.callFrames.shift();
      if (previous && !stack.callFrames.length)
        previous.parent = stack.parent;
      else
        previous = stack;
      stack = stack.parent;
    }
    return asyncStackTrace;
  }
};
