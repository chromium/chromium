// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @param {string} content
 */
FormatterWorker.javaScriptOutline = function(content) {
  const chunkSize = 100000;
  let outlineChunk = [];
  let lastReportedOffset = 0;

  let ast;
  try {
    ast = acorn.parse(content, {ranges: false, ecmaVersion: 8});
  } catch (e) {
    ast = acorn.loose.parse(content, {ranges: false, ecmaVersion: 8});
  }

  const textCursor = new TextUtils.TextCursor(content.computeLineEndings());
  const walker = new FormatterWorker.ESTreeWalker(beforeVisit);
  walker.walk(ast);
  postMessage({chunk: outlineChunk, isLastChunk: true});

  /**
   * @param {!ESTree.Node} node
   */
  function beforeVisit(node) {
    if (node.type === 'ClassDeclaration') {
      reportClass(/** @type {!ESTree.Node} */ (node.id));
    } else if (node.type === 'VariableDeclarator' && isClassNode(node.init)) {
      reportClass(/** @type {!ESTree.Node} */ (node.id));
    } else if (node.type === 'AssignmentExpression' && isNameNode(node.left) && isClassNode(node.right)) {
      reportClass(/** @type {!ESTree.Node} */ (node.left));
    } else if (node.type === 'Property' && isNameNode(node.key) && isClassNode(node.value)) {
      reportClass(/** @type {!ESTree.Node} */ (node.key));
    } else if (node.type === 'FunctionDeclaration') {
      reportFunction(/** @type {!ESTree.Node} */ (node.id), node);
    } else if (node.type === 'VariableDeclarator' && isFunctionNode(node.init)) {
      reportFunction(/** @type {!ESTree.Node} */ (node.id), /** @type {!ESTree.Node} */ (node.init));
    } else if (node.type === 'AssignmentExpression' && isNameNode(node.left) && isFunctionNode(node.right)) {
      reportFunction(/** @type {!ESTree.Node} */ (node.left), /** @type {!ESTree.Node} */ (node.right));
    } else if (
        (node.type === 'MethodDefinition' || node.type === 'Property') && isNameNode(node.key) &&
        isFunctionNode(node.value)) {
      const namePrefix = [];
      if (node.kind === 'get' || node.kind === 'set')
        namePrefix.push(node.kind);
      if (node.static)
        namePrefix.push('static');
      reportFunction(node.key, node.value, namePrefix.join(' '));
    }
  }

  /**
   * @param {!ESTree.Node} nameNode
   */
  function reportClass(nameNode) {
    const name = 'class ' + stringifyNameNode(nameNode);
    textCursor.advance(nameNode.start);
    addOutlineItem({
      name: name,
      line: textCursor.lineNumber(),
      column: textCursor.columnNumber(),
    });
  }

  /**
   * @param {!ESTree.Node} nameNode
   * @param {!ESTree.Node} functionNode
   * @param {string=} namePrefix
   */
  function reportFunction(nameNode, functionNode, namePrefix) {
    let name = stringifyNameNode(nameNode);
    if (functionNode.generator)
      name = '*' + name;
    if (namePrefix)
      name = namePrefix + ' ' + name;
    if (functionNode.async)
      name = 'async ' + name;

    textCursor.advance(nameNode.start);
    addOutlineItem({
      name: name,
      line: textCursor.lineNumber(),
      column: textCursor.columnNumber(),
      arguments: stringifyArguments(/** @type {!Array<!ESTree.Node>} */ (functionNode.params))
    });
  }

  /**
   * @param {(!ESTree.Node|undefined)} node
   * @return {boolean}
   */
  function isNameNode(node) {
    if (!node)
      return false;
    if (node.type === 'MemberExpression')
      return !node.computed && node.property.type === 'Identifier';
    return node.type === 'Identifier';
  }

  /**
   * @param {(!ESTree.Node|undefined)} node
   * @return {boolean}
   */
  function isFunctionNode(node) {
    if (!node)
      return false;
    return node.type === 'FunctionExpression' || node.type === 'ArrowFunctionExpression';
  }

  /**
   * @param {(!ESTree.Node|undefined)} node
   * @return {boolean}
   */
  function isClassNode(node) {
    return !!node && node.type === 'ClassExpression';
  }

  /**
   * @param {!ESTree.Node} node
   * @return {string}
   */
  function stringifyNameNode(node) {
    if (node.type === 'MemberExpression')
      node = /** @type {!ESTree.Node} */ (node.property);
    console.assert(node.type === 'Identifier', 'Cannot extract identifier from unknown type: ' + node.type);
    return /** @type {string} */ (node.name);
  }

  /**
   * @param {!Array<!ESTree.Node>} params
   * @return {string}
   */
  function stringifyArguments(params) {
    const result = [];
    for (const param of params) {
      if (param.type === 'Identifier')
        result.push(param.name);
      else if (param.type === 'RestElement' && param.argument.type === 'Identifier')
        result.push('...' + param.argument.name);
      else
        console.error('Error: unexpected function parameter type: ' + param.type);
    }
    return '(' + result.join(', ') + ')';
  }

  /**
   * @param {{name: string, line: number, column: number, arguments: (string|undefined)}} item
   */
  function addOutlineItem(item) {
    outlineChunk.push(item);
    if (textCursor.offset() - lastReportedOffset < chunkSize)
      return;
    postMessage({chunk: outlineChunk, isLastChunk: false});
    outlineChunk = [];
    lastReportedOffset = textCursor.offset();
  }
};
