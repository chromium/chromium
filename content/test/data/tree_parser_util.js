// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Parser for a simple grammar that describes a tree structure using a function-
 * like "a(b(c,d))" syntax. Original intended usage: to have browsertests
 * specify an arbitrary tree of iframes, loaded from various sites, without
 * having to write a .html page for each level or do crazy feats of data: url
 * escaping. But there's nothing really iframe-specific here. See below for some
 * examples of the grammar and the parser output.
 *
 * @example <caption>Basic syntax: an identifier, optionally followed by a list
 * of attributes, optionally followed by a list of children.</caption>
 * // returns { value: 'abc', attributes: [], children: [] }
 * TreeParserUtil.parse('abc {} ()');
 *
 * @example <caption>Both the attribute and child lists are optional. Dots and
 * hyphens are legal in ids.</caption>
 * // returns { value: 'example-b.com', attributes: [], children: [] }
 * TreeParserUtil.parse('example-b.com');
 *
 * @example <caption>Attributes are identifiers as well, separated by commas.
 * </caption>
 * // returns { value: 'abc', attributes: ['attr-1', 'attr-2'], children: [] }
 * TreeParserUtil.parse('abc {attr-1, attr-2}');
 *
 * @example <caption>Commas separate children in the child list.</caption>
 * // returns { value: 'b', attributes: [], children: [
 * //           { value: 'c', attributes: [], children: [] },
 * //           { value: 'd', attributes: [], children: [] }
 * //         ]}
 * TreeParserUtil.parse('b (c, d)';
 *
 * @example <caption>Children can have children, and so on.</caption>
 * // returns { value: 'e', attributes: [], children: [
 * //           { value: 'f', attributes: [], children: [
 * //             { value: 'g', attributes: [], children: [
 * //               { value: 'h', attributes: [], children: [] },
 * //               { value: 'i', attributes: [], children: [
 * //                 { value: 'j', attributes: [], children: [] }
 * //               ]},
 * //             ]}
 * //           ]}
 * //         ]}
 * TreeParserUtil.parse('e(f(g(h(),i(j))))';
 *
 * @example <caption>Attributes can be applied to children at any level of
 * nesting.</caption>
 * // returns { value: 'b', attributes: ['red', 'blue'], children: [
 * //           { value: 'c', attributes: [], children: [] },
 * //           { value: 'd', attributes: ['green'], children: [] }
 * //         ]}
 * TreeParserUtil.parse('b{red,blue}(c,d{green})';
 *
 * @example <caption>flatten() converts a [sub]tree back to a string.</caption>
 * var tree = TreeParserUtil.parse('b.com (c.com(e.com), d.com)');
 * TreeParserUtil.flatten(tree.children[0]);  // returns 'c.com(e.com())'
 */
var TreeParserUtil = (function() {
  'use strict';

  /**
   * Parses an input string into a tree. See class comment for examples.
   * @returns A tree of the form {value: <string>, children: <Array.<tree>>}.
   */
  function parse(input) {
    var tokenStream = lex(input);

    var result = takeIdAndChild(tokenStream);
    if (tokenStream.length != 0)
      throw new Error('Expected end of stream, but found "' +
                      tokenStream[0] + '".')
    return result;
  }

  /**
   * Inverse of |parse|. Converts a parsed tree object into a string. Can be
   * used to forward a subtree as an argument to a nested document.
   */
  function flatten(tree) {
    var result = tree.value;
    if (tree.attributes && tree.attributes.length)
      result += '{' + tree.attributes.join(",") + "}";
    return result + '(' + tree.children.map(flatten).join(',') + ')';
  }

  /**
   * Lexer function to convert an input string into a token stream.  Splits the
   * input along whitespace, parens and commas. Whitespace is removed, while
   * parens and commas are preserved as standalone tokens.
   *
   * @param {string} input The input string.
   * @return {Array.<string>} The resulting token stream.
   */
  function lex(input) {
    return input.split(/(\s+|\(|\)|{|}|,)/).reduce(
      function (resultArray, token) {
        var trimmed = token.trim();
        if (trimmed) {
          resultArray.push(trimmed);
        }
        return resultArray;
      }, []);
  }

  /**
   * Consumes from the stream an identifier with optional attribute and child
   * lists, returning its parsed representation.
   */
  function takeIdAndChild(tokenStream) {
    return { value: takeIdentifier(tokenStream),
             attributes: takeAttributeList(tokenStream),
             children: takeChildList(tokenStream) };
  }

  /**
   * Consumes from the stream an identifier, returning its value (as a string).
   */
  function takeIdentifier(tokenStream) {
    if (tokenStream.length == 0)
      throw new Error('Expected an identifier, but found end-of-stream.');
    var token = tokenStream.shift();
    // This regex includes slash, question mark, ampersand, and period in order
    // to accommodate full URLs, which may be passed to
    // cross_site_iframe_factory.html as leaf node locations. Colon is for port
    // numbers, which may also be specified. Semicolon and equals are for cookie
    // strings, which may sometimes be provided in URL query strings such as for
    // the EmbeddedTestServer default handler /set-cookie. Percent is for
    // allowing escaped squences in the URL, like %2F.
    const re = new RegExp('^[a-zA-Z0-9\\/%_?&;:.=-]+$');
    if (!token.match(re))
      throw new Error('Expected an identifier, but found "' + token + '".');
    return token;
  }

  /**
   * Consumes an optional attribute list from the token stream, returning a list
   * of the parsed attribute identifiers.
   */
  function takeAttributeList(tokenStream) {
    // Remove the next token from the stream if it matches |token|.
    function tryToEatA(token) {
      if (tokenStream[0] == token) {
        tokenStream.shift();
        return true;
      }
      return false;
    }

    // Bare identifier case, as in 'b' in the input '(a (b, c))'
    if (!tryToEatA('{'))
      return [];

    // Empty list case, as in 'b' in the input 'a (b {}, c)'.
    if (tryToEatA('}')) {
      return [];
    }

    // List with at least one entry.
    var result = [ takeIdentifier(tokenStream) ];

    // Additional entries allowed with comma.
    while (tryToEatA(',')) {
      result.push(takeIdentifier(tokenStream));
    }

    // End of list.
    if (tryToEatA('}')) {
      return result;
    }
    if (tokenStream.length == 0)
      throw new Error('Expected "}" or ",", but found end-of-stream.');
    throw new Error('Expected "}" or ",", but found "' + tokenStream[0] + '".');
  }

  /**
   * Consumes an optional child list from the token stream, returning a list of
   * the parsed children.
   */
  function takeChildList(tokenStream) {
    // Remove the next token from the stream if it matches |token|.
    function tryToEatA(token) {
      if (tokenStream[0] == token) {
        tokenStream.shift();
        return true;
      }
      return false;
    }

    // Bare identifier case, as in 'b' in the input '(a (b, c))'
    if (!tryToEatA('('))
      return [];

    // Empty list case, as in 'b' in the input 'a (b (), c)'.
    if (tryToEatA(')')) {
      return [];
    }

    // List with at least one entry.
    var result = [ takeIdAndChild(tokenStream) ];

    // Additional entries allowed with commas.
    while (tryToEatA(',')) {
      result.push(takeIdAndChild(tokenStream));
    }

    // End of list.
    if (tryToEatA(')')) {
      return result;
    }
    if (tokenStream.length == 0)
      throw new Error('Expected ")" or ",", but found end-of-stream.');
    throw new Error('Expected ")" or ",", but found "' + tokenStream[0] + '".');
  }

  return {
    parse:   parse,
    flatten: flatten
  };
})();

document.scriptExecuted = true;
