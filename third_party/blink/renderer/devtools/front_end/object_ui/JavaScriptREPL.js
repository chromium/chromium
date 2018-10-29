// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

ObjectUI.JavaScriptREPL = class {
  /**
   * @param {string} code
   * @return {string}
   */
  static wrapObjectLiteral(code) {
    // Only parenthesize what appears to be an object literal.
    if (!(/^\s*\{/.test(code) && /\}\s*$/.test(code)))
      return code;

    const parse = (async () => 0).constructor;
    try {
      // Check if the code can be interpreted as an expression.
      parse('return ' + code + ';');

      // No syntax error! Does it work parenthesized?
      const wrappedCode = '(' + code + ')';
      parse(wrappedCode);

      return wrappedCode;
    } catch (e) {
      return code;
    }
  }

  /**
   * @param {string} text
   * @return {!Promise<!{text: string, preprocessed: boolean}>}
   */
  static async preprocessExpression(text) {
    text = ObjectUI.JavaScriptREPL.wrapObjectLiteral(text);
    let preprocessed = false;
    if (text.indexOf('await') !== -1) {
      const preprocessedText = await Formatter.formatterWorkerPool().preprocessTopLevelAwaitExpressions(text);
      preprocessed = !!preprocessedText;
      text = preprocessedText || text;
    }
    return {text, preprocessed};
  }

  /**
   * @param {string} text
   * @param {boolean} throwOnSideEffect
   * @param {number=} timeout
   * @param {boolean=} allowErrors
   * @param {string=} objectGroup
   * @return {!Promise<!{preview: !DocumentFragment, result: ?SDK.RuntimeModel.EvaluationResult}>}
   */
  static async evaluateAndBuildPreview(text, throwOnSideEffect, timeout, allowErrors, objectGroup) {
    const executionContext = UI.context.flavor(SDK.ExecutionContext);
    const isTextLong = text.length > ObjectUI.JavaScriptREPL._MaxLengthForEvaluation;
    if (!text || !executionContext || (throwOnSideEffect && isTextLong))
      return {preview: createDocumentFragment(), result: null};

    const wrappedResult = await ObjectUI.JavaScriptREPL.preprocessExpression(text);
    const options = {
      expression: wrappedResult.text,
      generatePreview: true,
      includeCommandLineAPI: true,
      throwOnSideEffect: throwOnSideEffect,
      timeout: timeout,
      objectGroup: objectGroup
    };
    const result = await executionContext.evaluate(
        options, false /* userGesture */, wrappedResult.preprocessed /* awaitPromise */);
    const preview = ObjectUI.JavaScriptREPL._buildEvaluationPreview(result, allowErrors);
    return {preview, result};
  }

  /**
   * @param {!SDK.RuntimeModel.EvaluationResult} result
   * @param {boolean=} allowErrors
   * @return {!DocumentFragment}
   */
  static _buildEvaluationPreview(result, allowErrors) {
    const fragment = createDocumentFragment();
    if (result.error)
      return fragment;

    if (result.exceptionDetails && result.exceptionDetails.exception && result.exceptionDetails.exception.description) {
      const exception = result.exceptionDetails.exception.description;
      if (exception.startsWith('TypeError: ') || allowErrors)
        fragment.createChild('span').textContent = result.exceptionDetails.text + ' ' + exception;
      return fragment;
    }

    const formatter = new ObjectUI.RemoteObjectPreviewFormatter();
    const {preview, type, subtype, description} = result.object;
    if (preview && type === 'object' && subtype !== 'node') {
      formatter.appendObjectPreview(fragment, preview, false /* isEntry */);
    } else {
      const nonObjectPreview = formatter.renderPropertyPreview(type, subtype, description.trimEnd(400));
      fragment.appendChild(nonObjectPreview);
    }
    return fragment;
  }
};

/**
 * @const
 * @type {number}
 */
ObjectUI.JavaScriptREPL._MaxLengthForEvaluation = 2000;
