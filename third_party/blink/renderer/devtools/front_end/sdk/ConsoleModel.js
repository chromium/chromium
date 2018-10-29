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
 * @implements {SDK.TargetManager.Observer}
 */
SDK.ConsoleModel = class extends Common.Object {
  constructor() {
    super();

    /** @type {!Array.<!SDK.ConsoleMessage>} */
    this._messages = [];
    /** @type {!Map<!SDK.RuntimeModel, !Map<number, !SDK.ConsoleMessage>>} */
    this._messageByExceptionId = new Map();
    this._warnings = 0;
    this._errors = 0;
    this._pageLoadSequenceNumber = 0;

    SDK.targetManager.observeTargets(this);
  }

  /**
   * @override
   * @param {!SDK.Target} target
   */
  targetAdded(target) {
    const resourceTreeModel = target.model(SDK.ResourceTreeModel);
    if (!resourceTreeModel || resourceTreeModel.cachedResourcesLoaded()) {
      this._initTarget(target);
      return;
    }

    const eventListener = resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, () => {
      Common.EventTarget.removeEventListeners([eventListener]);
      this._initTarget(target);
    });
  }

  /**
   * @param {!SDK.Target} target
   */
  _initTarget(target) {
    const eventListeners = [];

    const cpuProfilerModel = target.model(SDK.CPUProfilerModel);
    if (cpuProfilerModel) {
      eventListeners.push(cpuProfilerModel.addEventListener(
          SDK.CPUProfilerModel.Events.ConsoleProfileStarted, this._consoleProfileStarted.bind(this, cpuProfilerModel)));
      eventListeners.push(cpuProfilerModel.addEventListener(
          SDK.CPUProfilerModel.Events.ConsoleProfileFinished,
          this._consoleProfileFinished.bind(this, cpuProfilerModel)));
    }

    const resourceTreeModel = target.model(SDK.ResourceTreeModel);
    if (resourceTreeModel && !target.parentTarget()) {
      eventListeners.push(resourceTreeModel.addEventListener(
          SDK.ResourceTreeModel.Events.MainFrameNavigated, this._mainFrameNavigated, this));
    }

    const runtimeModel = target.model(SDK.RuntimeModel);
    if (runtimeModel) {
      eventListeners.push(runtimeModel.addEventListener(
          SDK.RuntimeModel.Events.ExceptionThrown, this._exceptionThrown.bind(this, runtimeModel)));
      eventListeners.push(runtimeModel.addEventListener(
          SDK.RuntimeModel.Events.ExceptionRevoked, this._exceptionRevoked.bind(this, runtimeModel)));
      eventListeners.push(runtimeModel.addEventListener(
          SDK.RuntimeModel.Events.ConsoleAPICalled, this._consoleAPICalled.bind(this, runtimeModel)));
      if (!target.parentTarget()) {
        eventListeners.push(runtimeModel.debuggerModel().addEventListener(
            SDK.DebuggerModel.Events.GlobalObjectCleared, this._clearIfNecessary, this));
      }
      eventListeners.push(runtimeModel.addEventListener(
          SDK.RuntimeModel.Events.QueryObjectRequested, this._queryObjectRequested.bind(this, runtimeModel)));
    }

    target[SDK.ConsoleModel._events] = eventListeners;
  }

  /**
   * @override
   * @param {!SDK.Target} target
   */
  targetRemoved(target) {
    const runtimeModel = target.model(SDK.RuntimeModel);
    if (runtimeModel)
      this._messageByExceptionId.delete(runtimeModel);
    Common.EventTarget.removeEventListeners(target[SDK.ConsoleModel._events] || []);
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @param {!SDK.ConsoleMessage} originatingMessage
   * @param {string} expression
   * @param {boolean} useCommandLineAPI
   * @param {boolean} awaitPromise
   */
  async evaluateCommandInConsole(executionContext, originatingMessage, expression, useCommandLineAPI, awaitPromise) {
    const result = await executionContext.evaluate(
        {
          expression: expression,
          objectGroup: 'console',
          includeCommandLineAPI: useCommandLineAPI,
          silent: false,
          returnByValue: false,
          generatePreview: true
        },
        /* userGesture */ true, awaitPromise);
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.ConsoleEvaluated);
    if (result.error)
      return;
    await Common.console.showPromise();
    this.dispatchEventToListeners(
        SDK.ConsoleModel.Events.CommandEvaluated,
        {result: result.object, commandMessage: originatingMessage, exceptionDetails: result.exceptionDetails});
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @param {string} text
   * @return {!SDK.ConsoleMessage}
   */
  addCommandMessage(executionContext, text) {
    const commandMessage = new SDK.ConsoleMessage(
        executionContext.runtimeModel, SDK.ConsoleMessage.MessageSource.JS, null, text,
        SDK.ConsoleMessage.MessageType.Command);
    commandMessage.setExecutionContextId(executionContext.id);
    this.addMessage(commandMessage);
    return commandMessage;
  }

  /**
   * @param {!SDK.ConsoleMessage} msg
   */
  addMessage(msg) {
    if (msg.source === SDK.ConsoleMessage.MessageSource.Worker && SDK.targetManager.targetById(msg.workerId))
      return;

    msg._pageLoadSequenceNumber = this._pageLoadSequenceNumber;
    if (msg.source === SDK.ConsoleMessage.MessageSource.ConsoleAPI && msg.type === SDK.ConsoleMessage.MessageType.Clear)
      this._clearIfNecessary();

    this._messages.push(msg);
    const runtimeModel = msg.runtimeModel();
    if (msg._exceptionId && runtimeModel) {
      let modelMap = this._messageByExceptionId.get(runtimeModel);
      if (!modelMap) {
        modelMap = new Map();
        this._messageByExceptionId.set(runtimeModel, modelMap);
      }
      modelMap.set(msg._exceptionId, msg);
    }
    this._incrementErrorWarningCount(msg);
    this.dispatchEventToListeners(SDK.ConsoleModel.Events.MessageAdded, msg);
  }

  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {!Common.Event} event
   */
  _exceptionThrown(runtimeModel, event) {
    const exceptionWithTimestamp = /** @type {!SDK.RuntimeModel.ExceptionWithTimestamp} */ (event.data);
    const consoleMessage = SDK.ConsoleMessage.fromException(
        runtimeModel, exceptionWithTimestamp.details, undefined, exceptionWithTimestamp.timestamp, undefined);
    consoleMessage.setExceptionId(exceptionWithTimestamp.details.exceptionId);
    this.addMessage(consoleMessage);
  }

  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {!Common.Event} event
   */
  _exceptionRevoked(runtimeModel, event) {
    const exceptionId = /** @type {number} */ (event.data);
    const modelMap = this._messageByExceptionId.get(runtimeModel);
    const exceptionMessage = modelMap ? modelMap.get(exceptionId) : null;
    if (!exceptionMessage)
      return;
    this._errors--;
    exceptionMessage.level = SDK.ConsoleMessage.MessageLevel.Verbose;
    this.dispatchEventToListeners(SDK.ConsoleModel.Events.MessageUpdated, exceptionMessage);
  }

  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {!Common.Event} event
   */
  _consoleAPICalled(runtimeModel, event) {
    const call = /** @type {!SDK.RuntimeModel.ConsoleAPICall} */ (event.data);
    let level = SDK.ConsoleMessage.MessageLevel.Info;
    if (call.type === SDK.ConsoleMessage.MessageType.Debug)
      level = SDK.ConsoleMessage.MessageLevel.Verbose;
    else if (call.type === SDK.ConsoleMessage.MessageType.Error || call.type === SDK.ConsoleMessage.MessageType.Assert)
      level = SDK.ConsoleMessage.MessageLevel.Error;
    else if (call.type === SDK.ConsoleMessage.MessageType.Warning)
      level = SDK.ConsoleMessage.MessageLevel.Warning;
    else if (call.type === SDK.ConsoleMessage.MessageType.Info || call.type === SDK.ConsoleMessage.MessageType.Log)
      level = SDK.ConsoleMessage.MessageLevel.Info;
    let message = '';
    if (call.args.length && call.args[0].unserializableValue)
      message = call.args[0].unserializableValue;
    else if (call.args.length && (typeof call.args[0].value !== 'object' || call.args[0].value === null))
      message = call.args[0].value + '';
    else if (call.args.length && call.args[0].description)
      message = call.args[0].description;
    const callFrame = call.stackTrace && call.stackTrace.callFrames.length ? call.stackTrace.callFrames[0] : null;
    const consoleMessage = new SDK.ConsoleMessage(
        runtimeModel, SDK.ConsoleMessage.MessageSource.ConsoleAPI, level,
        /** @type {string} */ (message), call.type, callFrame ? callFrame.url : undefined,
        callFrame ? callFrame.lineNumber : undefined, callFrame ? callFrame.columnNumber : undefined, call.args,
        call.stackTrace, call.timestamp, call.executionContextId, undefined, undefined, call.context);
    this.addMessage(consoleMessage);
  }

  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {!Common.Event} event
   */
  _queryObjectRequested(runtimeModel, event) {
    const consoleMessage = new SDK.ConsoleMessage(
        runtimeModel, SDK.ConsoleMessage.MessageSource.ConsoleAPI, SDK.ConsoleMessage.MessageLevel.Info, '',
        SDK.ConsoleMessage.MessageType.QueryObjectResult, undefined, undefined, undefined, [event.data.objects]);
    this.addMessage(consoleMessage);
  }

  _clearIfNecessary() {
    if (!Common.moduleSetting('preserveConsoleLog').get())
      this._clear();
    ++this._pageLoadSequenceNumber;
  }

  /**
   * @param {!Common.Event} event
   */
  _mainFrameNavigated(event) {
    if (Common.moduleSetting('preserveConsoleLog').get())
      Common.console.log(Common.UIString('Navigated to %s', event.data.url));
  }

  /**
   * @param {!SDK.CPUProfilerModel} cpuProfilerModel
   * @param {!Common.Event} event
   */
  _consoleProfileStarted(cpuProfilerModel, event) {
    const data = /** @type {!SDK.CPUProfilerModel.EventData} */ (event.data);
    this._addConsoleProfileMessage(
        cpuProfilerModel, SDK.ConsoleMessage.MessageType.Profile, data.scriptLocation,
        Common.UIString('Profile \'%s\' started.', data.title));
  }

  /**
   * @param {!SDK.CPUProfilerModel} cpuProfilerModel
   * @param {!Common.Event} event
   */
  _consoleProfileFinished(cpuProfilerModel, event) {
    const data = /** @type {!SDK.CPUProfilerModel.EventData} */ (event.data);
    this._addConsoleProfileMessage(
        cpuProfilerModel, SDK.ConsoleMessage.MessageType.ProfileEnd, data.scriptLocation,
        Common.UIString('Profile \'%s\' finished.', data.title));
  }

  /**
   * @param {!SDK.CPUProfilerModel} cpuProfilerModel
   * @param {string} type
   * @param {!SDK.DebuggerModel.Location} scriptLocation
   * @param {string} messageText
   */
  _addConsoleProfileMessage(cpuProfilerModel, type, scriptLocation, messageText) {
    const stackTrace = [{
      functionName: '',
      scriptId: scriptLocation.scriptId,
      url: scriptLocation.script() ? scriptLocation.script().contentURL() : '',
      lineNumber: scriptLocation.lineNumber,
      columnNumber: scriptLocation.columnNumber || 0
    }];
    this.addMessage(new SDK.ConsoleMessage(
        cpuProfilerModel.runtimeModel(), SDK.ConsoleMessage.MessageSource.ConsoleAPI,
        SDK.ConsoleMessage.MessageLevel.Info, messageText, type, undefined, undefined, undefined, stackTrace));
  }

  /**
   * @param {!SDK.ConsoleMessage} msg
   */
  _incrementErrorWarningCount(msg) {
    if (msg.source === SDK.ConsoleMessage.MessageSource.Violation)
      return;
    switch (msg.level) {
      case SDK.ConsoleMessage.MessageLevel.Warning:
        this._warnings++;
        break;
      case SDK.ConsoleMessage.MessageLevel.Error:
        this._errors++;
        break;
    }
  }

  /**
   * @return {!Array.<!SDK.ConsoleMessage>}
   */
  messages() {
    return this._messages;
  }

  requestClearMessages() {
    for (const logModel of SDK.targetManager.models(SDK.LogModel))
      logModel.requestClear();
    for (const runtimeModel of SDK.targetManager.models(SDK.RuntimeModel))
      runtimeModel.discardConsoleEntries();
    this._clear();
  }

  _clear() {
    this._messages = [];
    this._messageByExceptionId.clear();
    this._errors = 0;
    this._warnings = 0;
    this.dispatchEventToListeners(SDK.ConsoleModel.Events.ConsoleCleared);
  }

  /**
   * @return {number}
   */
  errors() {
    return this._errors;
  }

  /**
   * @return {number}
   */
  warnings() {
    return this._warnings;
  }

  /**
   * @param {?SDK.ExecutionContext} currentExecutionContext
   * @param {?SDK.RemoteObject} remoteObject
   */
  async saveToTempVariable(currentExecutionContext, remoteObject) {
    if (!remoteObject || !currentExecutionContext) {
      failedToSave(null);
      return;
    }
    const executionContext = /** @type {!SDK.ExecutionContext} */ (currentExecutionContext);

    const result = await executionContext.globalObject(/* objectGroup */ '', /* generatePreview */ false);
    if (!!result.exceptionDetails || !result.object) {
      failedToSave(result.object || null);
      return;
    }

    const globalObject = result.object;
    const callFunctionResult =
        await globalObject.callFunction(saveVariable, [SDK.RemoteObject.toCallArgument(remoteObject)]);
    globalObject.release();
    if (callFunctionResult.wasThrown || !callFunctionResult.object || callFunctionResult.object.type !== 'string') {
      failedToSave(callFunctionResult.object || null);
    } else {
      const text = /** @type {string} */ (callFunctionResult.object.value);
      const message = this.addCommandMessage(executionContext, text);
      this.evaluateCommandInConsole(
          executionContext, message, text, /* useCommandLineAPI */ false, /* awaitPromise */ false);
    }
    if (callFunctionResult.object)
      callFunctionResult.object.release();

    /**
     * @suppressReceiverCheck
     * @this {Window}
     */
    function saveVariable(value) {
      const prefix = 'temp';
      let index = 1;
      while ((prefix + index) in this)
        ++index;
      const name = prefix + index;
      this[name] = value;
      return name;
    }

    /**
     * @param {?SDK.RemoteObject} result
     */
    function failedToSave(result) {
      let message = Common.UIString('Failed to save to temp variable.');
      if (result)
        message += ' ' + result.description;
      Common.console.error(message);
    }
  }
};

/** @enum {symbol} */
SDK.ConsoleModel.Events = {
  ConsoleCleared: Symbol('ConsoleCleared'),
  MessageAdded: Symbol('MessageAdded'),
  MessageUpdated: Symbol('MessageUpdated'),
  CommandEvaluated: Symbol('CommandEvaluated')
};


/**
 * @unrestricted
 */
SDK.ConsoleMessage = class {
  /**
   * @param {?SDK.RuntimeModel} runtimeModel
   * @param {string} source
   * @param {?string} level
   * @param {string} messageText
   * @param {string=} type
   * @param {?string=} url
   * @param {number=} line
   * @param {number=} column
   * @param {!Array.<!Protocol.Runtime.RemoteObject>=} parameters
   * @param {!Protocol.Runtime.StackTrace=} stackTrace
   * @param {number=} timestamp
   * @param {!Protocol.Runtime.ExecutionContextId=} executionContextId
   * @param {?string=} scriptId
   * @param {?string=} workerId
   * @param {string=} context
   */
  constructor(
      runtimeModel, source, level, messageText, type, url, line, column, parameters, stackTrace, timestamp,
      executionContextId, scriptId, workerId, context) {
    this._runtimeModel = runtimeModel;
    this.source = source;
    this.level = /** @type {?SDK.ConsoleMessage.MessageLevel} */ (level);
    this.messageText = messageText;
    this.type = type || SDK.ConsoleMessage.MessageType.Log;
    /** @type {string|undefined} */
    this.url = url || undefined;
    /** @type {number} */
    this.line = line || 0;
    /** @type {number} */
    this.column = column || 0;
    this.parameters = parameters;
    /** @type {!Protocol.Runtime.StackTrace|undefined} */
    this.stackTrace = stackTrace;
    this.timestamp = timestamp || Date.now();
    this.executionContextId = executionContextId || 0;
    this.scriptId = scriptId || null;
    this.workerId = workerId || null;

    if (!this.executionContextId && this._runtimeModel) {
      if (this.scriptId)
        this.executionContextId = this._runtimeModel.executionContextIdForScriptId(this.scriptId);
      else if (this.stackTrace)
        this.executionContextId = this._runtimeModel.executionContextForStackTrace(this.stackTrace);
    }

    if (context)
      this.context = context.match(/[^#]*/)[0];
  }

  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {!Protocol.Runtime.ExceptionDetails} exceptionDetails
   * @param {string=} messageType
   * @param {number=} timestamp
   * @param {string=} forceUrl
   * @return {!SDK.ConsoleMessage}
   */
  static fromException(runtimeModel, exceptionDetails, messageType, timestamp, forceUrl) {
    return new SDK.ConsoleMessage(
        runtimeModel, SDK.ConsoleMessage.MessageSource.JS, SDK.ConsoleMessage.MessageLevel.Error,
        SDK.RuntimeModel.simpleTextFromException(exceptionDetails), messageType, forceUrl || exceptionDetails.url,
        exceptionDetails.lineNumber, exceptionDetails.columnNumber,
        exceptionDetails.exception ?
            [SDK.RemoteObject.fromLocalObject(exceptionDetails.text), exceptionDetails.exception] :
            undefined,
        exceptionDetails.stackTrace, timestamp, exceptionDetails.executionContextId, exceptionDetails.scriptId);
  }

  /**
   * @return {?SDK.RuntimeModel}
   */
  runtimeModel() {
    return this._runtimeModel;
  }

  /**
   * @return {?SDK.Target}
   */
  target() {
    return this._runtimeModel ? this._runtimeModel.target() : null;
  }

  /**
   * @param {!SDK.ConsoleMessage} originatingMessage
   */
  setOriginatingMessage(originatingMessage) {
    this._originatingConsoleMessage = originatingMessage;
    this.executionContextId = originatingMessage.executionContextId;
  }

  /**
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   */
  setExecutionContextId(executionContextId) {
    this.executionContextId = executionContextId;
  }

  /**
   * @param {number} exceptionId
   */
  setExceptionId(exceptionId) {
    this._exceptionId = exceptionId;
  }

  /**
   * @return {?SDK.ConsoleMessage}
   */
  originatingMessage() {
    return this._originatingConsoleMessage;
  }

  /**
   * @return {boolean}
   */
  isGroupMessage() {
    return this.type === SDK.ConsoleMessage.MessageType.StartGroup ||
        this.type === SDK.ConsoleMessage.MessageType.StartGroupCollapsed ||
        this.type === SDK.ConsoleMessage.MessageType.EndGroup;
  }

  /**
   * @return {boolean}
   */
  isGroupStartMessage() {
    return this.type === SDK.ConsoleMessage.MessageType.StartGroup ||
        this.type === SDK.ConsoleMessage.MessageType.StartGroupCollapsed;
  }

  /**
   * @return {boolean}
   */
  isErrorOrWarning() {
    return (
        this.level === SDK.ConsoleMessage.MessageLevel.Warning || this.level === SDK.ConsoleMessage.MessageLevel.Error);
  }

  /**
   * @return {boolean}
   */
  isGroupable() {
    const isUngroupableError = this.level === SDK.ConsoleMessage.MessageLevel.Error &&
        (this.source === SDK.ConsoleMessage.MessageSource.JS ||
         this.source === SDK.ConsoleMessage.MessageSource.Network);
    return (
        this.source !== SDK.ConsoleMessage.MessageSource.ConsoleAPI &&
        this.type !== SDK.ConsoleMessage.MessageType.Command && this.type !== SDK.ConsoleMessage.MessageType.Result &&
        this.type !== SDK.ConsoleMessage.MessageType.System && !isUngroupableError);
  }

  /**
   * @return {string}
   */
  groupCategoryKey() {
    return [this.source, this.level, this.type, this._pageLoadSequenceNumber].join(':');
  }

  /**
   * @param {?SDK.ConsoleMessage} msg
   * @return {boolean}
   */
  isEqual(msg) {
    if (!msg)
      return false;

    if (!this._isEqualStackTraces(this.stackTrace, msg.stackTrace))
      return false;

    if (this.parameters) {
      if (!msg.parameters || this.parameters.length !== msg.parameters.length)
        return false;

      for (let i = 0; i < msg.parameters.length; ++i) {
        // Never treat objects as equal - their properties might change over time. Errors can be treated as equal
        // since they are always formatted as strings.
        if (msg.parameters[i].type === 'object' && msg.parameters[i].subtype !== 'error')
          return false;
        if (this.parameters[i].type !== msg.parameters[i].type ||
            this.parameters[i].value !== msg.parameters[i].value ||
            this.parameters[i].description !== msg.parameters[i].description)
          return false;
      }
    }

    return (this.runtimeModel() === msg.runtimeModel()) && (this.source === msg.source) && (this.type === msg.type) &&
        (this.level === msg.level) && (this.line === msg.line) && (this.url === msg.url) &&
        (this.messageText === msg.messageText) && (this.request === msg.request) &&
        (this.executionContextId === msg.executionContextId);
  }

  /**
   * @param {!Protocol.Runtime.StackTrace|undefined} stackTrace1
   * @param {!Protocol.Runtime.StackTrace|undefined} stackTrace2
   * @return {boolean}
   */
  _isEqualStackTraces(stackTrace1, stackTrace2) {
    if (!stackTrace1 !== !stackTrace2)
      return false;
    if (!stackTrace1)
      return true;
    const callFrames1 = stackTrace1.callFrames;
    const callFrames2 = stackTrace2.callFrames;
    if (callFrames1.length !== callFrames2.length)
      return false;
    for (let i = 0, n = callFrames1.length; i < n; ++i) {
      if (callFrames1[i].url !== callFrames2[i].url || callFrames1[i].functionName !== callFrames2[i].functionName ||
          callFrames1[i].lineNumber !== callFrames2[i].lineNumber ||
          callFrames1[i].columnNumber !== callFrames2[i].columnNumber)
        return false;
    }
    return this._isEqualStackTraces(stackTrace1.parent, stackTrace2.parent);
  }
};

// Note: Keep these constants in sync with the ones in ConsoleTypes.h
/**
 * @enum {string}
 */
SDK.ConsoleMessage.MessageSource = {
  XML: 'xml',
  JS: 'javascript',
  Network: 'network',
  ConsoleAPI: 'console-api',
  Storage: 'storage',
  AppCache: 'appcache',
  Rendering: 'rendering',
  CSS: 'css',
  Security: 'security',
  Deprecation: 'deprecation',
  Worker: 'worker',
  Violation: 'violation',
  Intervention: 'intervention',
  Recommendation: 'recommendation',
  Other: 'other'
};

/**
 * @enum {string}
 */
SDK.ConsoleMessage.MessageType = {
  Log: 'log',
  Debug: 'debug',
  Info: 'info',
  Error: 'error',
  Warning: 'warning',
  Dir: 'dir',
  DirXML: 'dirxml',
  Table: 'table',
  Trace: 'trace',
  Clear: 'clear',
  StartGroup: 'startGroup',
  StartGroupCollapsed: 'startGroupCollapsed',
  EndGroup: 'endGroup',
  Assert: 'assert',
  Result: 'result',
  Profile: 'profile',
  ProfileEnd: 'profileEnd',
  Command: 'command',
  System: 'system',
  QueryObjectResult: 'queryObjectResult'
};

/**
 * @enum {string}
 */
SDK.ConsoleMessage.MessageLevel = {
  Verbose: 'verbose',
  Info: 'info',
  Warning: 'warning',
  Error: 'error'
};

/** @type {!Map<!SDK.ConsoleMessage.MessageSource, string>} */
SDK.ConsoleMessage.MessageSourceDisplayName = new Map([
  [SDK.ConsoleMessage.MessageSource.XML, 'xml'], [SDK.ConsoleMessage.MessageSource.JS, 'javascript'],
  [SDK.ConsoleMessage.MessageSource.Network, 'network'], [SDK.ConsoleMessage.MessageSource.ConsoleAPI, 'console-api'],
  [SDK.ConsoleMessage.MessageSource.Storage, 'storage'], [SDK.ConsoleMessage.MessageSource.AppCache, 'appcache'],
  [SDK.ConsoleMessage.MessageSource.Rendering, 'rendering'], [SDK.ConsoleMessage.MessageSource.CSS, 'css'],
  [SDK.ConsoleMessage.MessageSource.Security, 'security'],
  [SDK.ConsoleMessage.MessageSource.Deprecation, 'deprecation'], [SDK.ConsoleMessage.MessageSource.Worker, 'worker'],
  [SDK.ConsoleMessage.MessageSource.Violation, 'violation'],
  [SDK.ConsoleMessage.MessageSource.Intervention, 'intervention'],
  [SDK.ConsoleMessage.MessageSource.Recommendation, 'recommendation'],
  [SDK.ConsoleMessage.MessageSource.Other, 'other']
]);

SDK.ConsoleModel._events = Symbol('SDK.ConsoleModel.events');

/**
 * @type {!SDK.ConsoleModel}
 */
SDK.consoleModel;
