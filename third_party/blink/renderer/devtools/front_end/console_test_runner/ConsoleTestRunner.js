// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview using private properties isn't a Closure violation in tests.
 * @suppress {accessControls}
 */

/** @typedef {function(!Element, !SDK.ConsoleMessage=):string} */
ConsoleTestRunner.Formatter;

/**
 * @param {boolean=} printOriginatingCommand
 * @param {boolean=} dumpClassNames
 * @param {!ConsoleTestRunner.Formatter=} formatter
 */
ConsoleTestRunner.dumpConsoleMessages = function(printOriginatingCommand, dumpClassNames, formatter) {
  TestRunner.addResults(
      ConsoleTestRunner.dumpConsoleMessagesIntoArray(printOriginatingCommand, dumpClassNames, formatter));
};

ConsoleTestRunner.renderCompleteMessages = async function() {
  const consoleView = Console.ConsoleView.instance();
  if (consoleView._needsFullUpdate)
    consoleView._updateMessageList();
  const viewMessages = consoleView._visibleViewMessages;
  await Promise.all(viewMessages.map(uiMessage => uiMessage.completeElementForTest()));
};

/**
 * @param {boolean=} printOriginatingCommand
 * @param {boolean=} dumpClassNames
 * @param {!ConsoleTestRunner.Formatter=} formatter
 * @return {!Array<string>}
 */
ConsoleTestRunner.dumpConsoleMessagesIntoArray = function(printOriginatingCommand, dumpClassNames, formatter) {
  formatter = formatter || ConsoleTestRunner.prepareConsoleMessageText;
  const result = [];
  ConsoleTestRunner.disableConsoleViewport();
  const consoleView = Console.ConsoleView.instance();
  if (consoleView._needsFullUpdate)
    consoleView._updateMessageList();
  const viewMessages = consoleView._visibleViewMessages;
  for (let i = 0; i < viewMessages.length; ++i) {
    const uiMessage = viewMessages[i];
    const message = uiMessage.consoleMessage();
    const element = uiMessage.element();

    let classNames;
    if (dumpClassNames) {
      classNames = [''];
      for (let node = element.firstChild; node; node = node.traverseNextNode(element)) {
        if (node.nodeType === Node.ELEMENT_NODE && node.className) {
          let depth = 0;
          let depthTest = node;
          while (depthTest !== element) {
            if (depthTest.nodeType === Node.ELEMENT_NODE && depthTest.className)
              depth++;
            depthTest = depthTest.parentNodeOrShadowHost();
          }
          classNames.push(
              '  '.repeat(depth) +
              node.className.replace('platform-linux', 'platform-*')
                  .replace('platform-mac', 'platform-*')
                  .replace('platform-windows', 'platform-*'));
        }
      }
    }

    if (ConsoleTestRunner.dumpConsoleTableMessage(uiMessage, false, result)) {
      if (dumpClassNames)
        result.push(classNames.join('\n'));
    } else {
      let messageText = formatter(element, message);
      messageText = messageText.replace(/VM\d+/g, 'VM');
      result.push(messageText + (dumpClassNames ? ' ' + classNames.join('\n') : ''));
    }

    if (printOriginatingCommand && uiMessage.consoleMessage().originatingMessage())
      result.push('Originating from: ' + uiMessage.consoleMessage().originatingMessage().messageText);
  }
  return result;
};

/**
 * @param {!Element} messageElement
 * @return {string}
 */
ConsoleTestRunner.prepareConsoleMessageText = function(messageElement) {
  let messageText = messageElement.deepTextContent().replace(/\u200b/g, '');
  // Replace scriptIds with generic scriptId string to avoid flakiness.
  messageText = messageText.replace(/VM\d+/g, 'VM');
  // Remove line and column of evaluate method.
  messageText = messageText.replace(/(at eval \(eval at evaluate) \(:\d+:\d+\)/, '$1');

  if (messageText.startsWith('Navigated to')) {
    const fileName = messageText.split(' ').pop().split('/').pop();
    messageText = 'Navigated to ' + fileName;
  }
  // The message might be extremely long in case of dumping stack overflow message.
  messageText = messageText.substring(0, 1024);
  return messageText;
};

/**
 * @param {!Console.ConsoleViewMessage} viewMessage
 * @param {boolean} forceInvalidate
 * @param {!Array<string>} results
 * @return {boolean}
 */
ConsoleTestRunner.dumpConsoleTableMessage = function(viewMessage, forceInvalidate, results) {
  if (forceInvalidate)
    Console.ConsoleView.instance()._viewport.invalidate();
  const table = viewMessage.element();
  const headers = table.querySelectorAll('th > div:first-child');
  if (!headers.length)
    return false;

  let headerLine = '';
  for (let i = 0; i < headers.length; i++)
    headerLine += headers[i].textContent + ' | ';

  addResult('HEADER ' + headerLine);

  const rows = table.querySelectorAll('.data-container tr');

  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    let rowLine = '';
    const items = row.querySelectorAll('td > span');
    for (let j = 0; j < items.length; j++)
      rowLine += items[j].textContent + ' | ';

    if (rowLine.trim())
      addResult('ROW ' + rowLine);
  }

  /**
   * @param {string} x
   */
  function addResult(x) {
    if (results)
      results.push(x);
    else
      TestRunner.addResult(x);
  }

  return true;
};

ConsoleTestRunner.disableConsoleViewport = function() {
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 2000);
};

/**
 * @param {number} width
 * @param {number} height
 */
ConsoleTestRunner.fixConsoleViewportDimensions = function(width, height) {
  const viewport = Console.ConsoleView.instance()._viewport;
  viewport.element.style.width = width + 'px';
  viewport.element.style.height = height + 'px';
  viewport.element.style.position = 'absolute';
  viewport.invalidate();
};

ConsoleTestRunner.selectMainExecutionContext = function() {
  const executionContexts = TestRunner.runtimeModel.executionContexts();
  for (const context of executionContexts) {
    if (context.isDefault) {
      UI.context.setFlavor(SDK.ExecutionContext, context);
      return;
    }
  }
};

/**
 * @param {string} code
 * @param {!Function=} callback
 * @param {boolean=} dontForceMainContext
 */
ConsoleTestRunner.evaluateInConsole = function(code, callback, dontForceMainContext) {
  if (!dontForceMainContext)
    ConsoleTestRunner.selectMainExecutionContext();
  callback = TestRunner.safeWrap(callback);

  const consoleView = Console.ConsoleView.instance();
  consoleView._prompt._appendCommand(code, true);
  ConsoleTestRunner.addConsoleViewSniffer(function(commandResult) {
    callback(commandResult.toMessageElement().deepTextContent());
  });
};

/**
 * @param {string} code
 * @param {boolean=} dontForceMainContext
 * @return {!Promise}
 */
ConsoleTestRunner.evaluateInConsolePromise = function(code, dontForceMainContext) {
  return new Promise(fulfill => ConsoleTestRunner.evaluateInConsole(code, fulfill, dontForceMainContext));
};

/**
 * @param {!Function} override
 * @param {boolean=} opt_sticky
 */
ConsoleTestRunner.addConsoleViewSniffer = function(override, opt_sticky) {
  TestRunner.addSniffer(Console.ConsoleView.prototype, '_consoleMessageAddedForTest', override, opt_sticky);
};

ConsoleTestRunner.waitForPendingViewportUpdates = async function() {
  const refreshPromise = Console.ConsoleView.instance()._scheduledRefreshPromiseForTest || Promise.resolve();
  await refreshPromise;
};

/**
 * @param {string} code
 * @param {!Function=} callback
 * @param {boolean=} dontForceMainContext
 */
ConsoleTestRunner.evaluateInConsoleAndDump = function(code, callback, dontForceMainContext) {
  callback = TestRunner.safeWrap(callback);
  /**
   * @param {string} text
   */
  function mycallback(text) {
    text = text.replace(/\bVM\d+/g, 'VM');
    TestRunner.addResult(code + ' = ' + text);
    callback(text);
  }
  ConsoleTestRunner.evaluateInConsole(code, mycallback, dontForceMainContext);
};

/**
 * @return {number}
 */
ConsoleTestRunner.consoleMessagesCount = function() {
  const consoleView = Console.ConsoleView.instance();
  return consoleView._consoleMessages.length;
};

/**
 * @param {function(!Element):string|undefined} messageFormatter
 * @param {!Element} node
 * @return {string}
 */
ConsoleTestRunner.formatterIgnoreStackFrameUrls = function(messageFormatter, node) {
  /**
   * @param {string} string
   */
  function isNotEmptyLine(string) {
    return string.trim().length > 0;
  }

  /**
   * @param {string} string
   */
  function ignoreStackFrameAndMutableData(string) {
    let buffer = string.replace(/\u200b/g, '');
    buffer = buffer.replace(/VM\d+/g, 'VM');
    return buffer.replace(/^\s+at [^\]]+(]?)$/, '$1');
  }

  messageFormatter = messageFormatter || TestRunner.textContentWithLineBreaks;
  const buffer = messageFormatter(node);
  return buffer.split('\n').map(ignoreStackFrameAndMutableData).filter(isNotEmptyLine).join('\n');
};

/**
 * @param {!Element} element
 * @param {!SDK.ConsoleMessage} message
 * @return {string}
 */
ConsoleTestRunner.simpleFormatter = function(element, message) {
  return message.messageText + ':' + message.line + ':' + message.column;
};

/**
 * @param {boolean=} printOriginatingCommand
 * @param {boolean=} dumpClassNames
 * @param {!ConsoleTestRunner.Formatter=} messageFormatter
 */
ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames = function(
    printOriginatingCommand, dumpClassNames, messageFormatter) {
  TestRunner.addResults(ConsoleTestRunner.dumpConsoleMessagesIntoArray(
      printOriginatingCommand, dumpClassNames,
      ConsoleTestRunner.formatterIgnoreStackFrameUrls.bind(this, messageFormatter)));
};

ConsoleTestRunner.dumpConsoleMessagesWithStyles = function() {
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;
  for (let i = 0; i < messageViews.length; ++i) {
    const element = messageViews[i].element();
    const messageText = ConsoleTestRunner.prepareConsoleMessageText(element);
    TestRunner.addResult(messageText);
    const spans = element.querySelectorAll('.console-message-text *');
    for (let j = 0; j < spans.length; ++j)
      TestRunner.addResult('Styled text #' + j + ': ' + (spans[j].style.cssText || 'NO STYLES DEFINED'));
  }
};

/**
 * @param {boolean=} sortMessages
 */
ConsoleTestRunner.dumpConsoleMessagesWithClasses = function(sortMessages) {
  const result = [];
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;
  for (let i = 0; i < messageViews.length; ++i) {
    const element = messageViews[i].element();
    const contentElement = messageViews[i].contentElement();
    const messageText = ConsoleTestRunner.prepareConsoleMessageText(element);
    result.push(messageText + ' ' + element.getAttribute('class') + ' > ' + contentElement.getAttribute('class'));
  }
  if (sortMessages)
    result.sort();
  TestRunner.addResults(result);
};

ConsoleTestRunner.dumpConsoleClassesBrief = function() {
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;
  for (let i = 0; i < messageViews.length; ++i) {
    const repeatText = messageViews[i].repeatCount() > 1 ? (' x' + messageViews[i].repeatCount()) : '';
    TestRunner.addResult(messageViews[i].toMessageElement().className + repeatText);
  }
};

ConsoleTestRunner.dumpConsoleCounters = async function() {
  const counter = ConsoleCounters.WarningErrorCounter._instanceForTest;
  if (counter._updatingForTest)
    await TestRunner.addSnifferPromise(counter, '_updatedForTest');
  for (let index = 0; index < counter._titles.length; ++index)
    TestRunner.addResult(counter._titles[index]);
  ConsoleTestRunner.dumpConsoleClassesBrief();
};

/**
 * @param {!Function} callback
 * @param {function(!Element):boolean} deepFilter
 * @param {function(!ObjectUI.ObjectPropertiesSection):boolean} sectionFilter
 */
ConsoleTestRunner.expandConsoleMessages = function(callback, deepFilter, sectionFilter) {
  Console.ConsoleView.instance()._invalidateViewport();
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;

  // Initiate round-trips to fetch necessary data for further rendering.
  for (let i = 0; i < messageViews.length; ++i)
    messageViews[i].element();

  TestRunner.deprecatedRunAfterPendingDispatches(expandTreeElements);

  function expandTreeElements() {
    for (let i = 0; i < messageViews.length; ++i) {
      const element = messageViews[i].element();
      for (let node = element; node; node = node.traverseNextNode(element)) {
        if (node.treeElementForTest)
          node.treeElementForTest.expand();
        if (node._expandStackTraceForTest)
          node._expandStackTraceForTest();
        if (!node._section)
          continue;
        if (sectionFilter && !sectionFilter(node._section))
          continue;
        node._section.expand();

        if (!deepFilter)
          continue;
        const treeElements = node._section.rootElement().children();
        for (let j = 0; j < treeElements.length; ++j) {
          for (let treeElement = treeElements[j]; treeElement;
               treeElement = treeElement.traverseNextTreeElement(true, null, true)) {
            if (deepFilter(treeElement))
              treeElement.expand();
          }
        }
      }
    }
    TestRunner.deprecatedRunAfterPendingDispatches(callback);
  }
};

/**
 * @param {function(!Element):boolean} deepFilter
 * @param {function(!ObjectUI.ObjectPropertiesSection):boolean} sectionFilter
 * @return {!Promise}
 */
ConsoleTestRunner.expandConsoleMessagesPromise = function(deepFilter, sectionFilter) {
  return new Promise(fulfill => ConsoleTestRunner.expandConsoleMessages(fulfill, deepFilter, sectionFilter));
};

/**
 * @param {!Function} callback
 */
ConsoleTestRunner.expandGettersInConsoleMessages = function(callback) {
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;
  const properties = [];
  let propertiesCount = 0;
  TestRunner.addSniffer(ObjectUI.ObjectPropertyTreeElement.prototype, '_updateExpandable', propertyExpandableUpdated);
  for (let i = 0; i < messageViews.length; ++i) {
    const element = messageViews[i].element();
    for (let node = element; node; node = node.traverseNextNode(element)) {
      if (node.classList && node.classList.contains('object-value-calculate-value-button')) {
        ++propertiesCount;
        node.click();
        properties.push(node.parentElement.parentElement);
      }
    }
  }

  function propertyExpandableUpdated() {
    --propertiesCount;
    if (propertiesCount === 0) {
      for (let i = 0; i < properties.length; ++i)
        properties[i].click();
      TestRunner.deprecatedRunAfterPendingDispatches(callback);
    } else {
      TestRunner.addSniffer(
          ObjectUI.ObjectPropertyTreeElement.prototype, '_updateExpandable', propertyExpandableUpdated);
    }
  }
};

/**
 * @param {!Function} callback
 */
ConsoleTestRunner.expandConsoleMessagesErrorParameters = function(callback) {
  const messageViews = Console.ConsoleView.instance()._visibleViewMessages;
  // Initiate round-trips to fetch necessary data for further rendering.
  for (let i = 0; i < messageViews.length; ++i)
    messageViews[i].element();
  TestRunner.deprecatedRunAfterPendingDispatches(callback);
};

/**
 * @param {!Function} callback
 */
ConsoleTestRunner.waitForRemoteObjectsConsoleMessages = function(callback) {
  const messages = Console.ConsoleView.instance()._visibleViewMessages;
  for (let i = 0; i < messages.length; ++i)
    messages[i].toMessageElement();
  TestRunner.deprecatedRunAfterPendingDispatches(callback);
};

/**
 * @return {!Promise}
 */
ConsoleTestRunner.waitForRemoteObjectsConsoleMessagesPromise = function() {
  return new Promise(resolve => ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(resolve));
};

/**
 * @return {!Promise}
 */
ConsoleTestRunner.waitUntilConsoleEditorLoaded = function() {
  let fulfill;
  const promise = new Promise(x => (fulfill = x));
  const prompt = Console.ConsoleView.instance()._prompt;
  if (prompt._editor)
    fulfill(prompt._editor);
  else
    TestRunner.addSniffer(Console.ConsolePrompt.prototype, '_editorSetForTest', _ => fulfill(prompt._editor));
  return promise;
};

/**
 * @param {!Function} callback
 */
ConsoleTestRunner.waitUntilMessageReceived = function(callback) {
  TestRunner.addSniffer(SDK.consoleModel, 'addMessage', callback, false);
};

/**
 * @return {!Promise}
 */
ConsoleTestRunner.waitUntilMessageReceivedPromise = function() {
  return new Promise(fulfill => ConsoleTestRunner.waitUntilMessageReceived(fulfill));
};

/**
 * @param {number} count
 * @param {!Function} callback
 */
ConsoleTestRunner.waitUntilNthMessageReceived = function(count, callback) {
  function override() {
    if (--count === 0)
      TestRunner.safeWrap(callback)();
    else
      TestRunner.addSniffer(SDK.consoleModel, 'addMessage', override, false);
  }
  TestRunner.addSniffer(SDK.consoleModel, 'addMessage', override, false);
};

/**
 * @param {number} count
 * @return {!Promise}
 */
ConsoleTestRunner.waitUntilNthMessageReceivedPromise = function(count) {
  return new Promise(fulfill => ConsoleTestRunner.waitUntilNthMessageReceived(count, fulfill));
};

/**
 * @param {string} namePrefix
 */
ConsoleTestRunner.changeExecutionContext = function(namePrefix) {
  const selector = Console.ConsoleView.instance()._consoleContextSelector;
  for (const executionContext of selector._items) {
    if (selector.titleFor(executionContext).startsWith(namePrefix)) {
      UI.context.setFlavor(SDK.ExecutionContext, executionContext);
      return;
    }
  }
  TestRunner.addResult('FAILED: context with prefix: ' + namePrefix + ' not found in the context list');
};

/**
 * @param {number} expectedCount
 * @param {!Function} callback
 */
ConsoleTestRunner.waitForConsoleMessages = function(expectedCount, callback) {
  const consoleView = Console.ConsoleView.instance();
  checkAndReturn();

  function checkAndReturn() {
    if (consoleView._visibleViewMessages.length === expectedCount) {
      TestRunner.addResult('Message count: ' + expectedCount);
      callback();
    } else {
      TestRunner.addSniffer(consoleView, '_messageAppendedForTests', checkAndReturn);
    }
  }
};

/**
 * @param {number} expectedCount
 * @return {!Promise}
 */
ConsoleTestRunner.waitForConsoleMessagesPromise = async function(expectedCount) {
  await new Promise(fulfill => ConsoleTestRunner.waitForConsoleMessages(expectedCount, fulfill));
  return ConsoleTestRunner.renderCompleteMessages();
};

/**
 * @param {number} fromMessage
 * @param {number} fromTextOffset
 * @param {number} toMessage
 * @param {number} toTextOffset
 * @suppressGlobalPropertiesCheck
 */
ConsoleTestRunner.selectConsoleMessages = function(fromMessage, fromTextOffset, toMessage, toTextOffset) {
  const consoleView = Console.ConsoleView.instance();
  const from = selectionContainerAndOffset(consoleView.itemElement(fromMessage).element(), fromTextOffset);
  const to = selectionContainerAndOffset(consoleView.itemElement(toMessage).element(), toTextOffset);
  window.getSelection().setBaseAndExtent(from.container, from.offset, to.container, to.offset);

  /**
   * @param {!Node} container
   * @param {number} offset
   * @return {?{container: !Node, offset: number}}
   */
  function selectionContainerAndOffset(container, offset) {
    /** @type {?Node} */
    let node = container;
    if (offset === 0 && container.nodeType !== Node.TEXT_NODE) {
      container = /** @type {!Node} */ (container.traverseNextTextNode());
      node = container;
    }
    let charCount = 0;
    while ((node = node.traverseNextTextNode(container))) {
      const length = node.textContent.length;
      if (charCount + length >= offset)
        return {container: node, offset: offset - charCount};

      charCount += length;
    }
    return null;
  }
};

/**
 * @param {!Function} override
 * @param {boolean=} opt_sticky
 */
ConsoleTestRunner.addConsoleSniffer = function(override, opt_sticky) {
  TestRunner.addSniffer(SDK.ConsoleModel.prototype, 'addMessage', override, opt_sticky);
};

/**
 * @param {!Function} func
 * @return {!Function}
 */
ConsoleTestRunner.wrapListener = function(func) {
  /**
   * @this {*}
   */
  async function wrapper() {
    await Promise.resolve();
    func.apply(this, arguments);
  }
  return wrapper;
};

ConsoleTestRunner.dumpStackTraces = function() {
  const viewMessages = Console.ConsoleView.instance()._visibleViewMessages;
  for (let i = 0; i < viewMessages.length; ++i) {
    const m = viewMessages[i].consoleMessage();
    TestRunner.addResult(
        'Message[' + i + ']: ' + Bindings.displayNameForURL(m.url || '') + ':' + m.line + ' ' + m.messageText);
    const trace = m.stackTrace ? m.stackTrace.callFrames : null;
    if (!trace) {
      TestRunner.addResult('FAIL: no stack trace attached to message #' + i);
    } else {
      TestRunner.addResult('Stack Trace:\n');
      TestRunner.addResult('  url: ' + trace[0].url);
      TestRunner.addResult('  function: ' + trace[0].functionName);
      TestRunner.addResult('  line: ' + trace[0].lineNumber);
    }
  }
};

/**
 * @return {!{first: number, last: number, count: number}}
 */
ConsoleTestRunner.visibleIndices = function() {
  const consoleView = Console.ConsoleView.instance();
  const viewport = consoleView._viewport;
  const viewportRect = viewport.element.getBoundingClientRect();
  let first = -1;
  let last = -1;
  let count = 0;
  for (let i = 0; i < consoleView._visibleViewMessages.length; i++) {
    // Created message elements may have a bounding rect, but not be connected to DOM.
    const item = consoleView._visibleViewMessages[i];
    if (!item._element || !item._element.isConnected)
      continue;
    const itemRect = item._element.getBoundingClientRect();
    const isVisible = (itemRect.bottom > viewportRect.top + 1) && (itemRect.top <= viewportRect.bottom - 1);
    if (isVisible) {
      first = first === -1 ? i : first;
      last = i;
      count++;
    }
  }
  return {first, last, count};
};
