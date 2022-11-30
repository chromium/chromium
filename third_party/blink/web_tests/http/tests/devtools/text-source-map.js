// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify TextSourceMap implementation\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');

  function checkMapping(
      compiledLineNumber, compiledColumnNumber, sourceURL, sourceLineNumber, sourceColumnNumber, mapping) {
    var entry = mapping.findEntry(compiledLineNumber, compiledColumnNumber);
    TestRunner.addResult(sourceURL + ' === ' + entry.sourceURL);
    TestRunner.addResult(sourceLineNumber + ' === ' + entry.sourceLineNumber);
    TestRunner.addResult(sourceColumnNumber + ' === ' + entry.sourceColumnNumber);
  }

  function checkReverseMapping(compiledLineNumber, compiledColumnNumber, sourceURL, sourceLineNumber, mapping) {
    var entry = mapping.sourceLineMapping(sourceURL, sourceLineNumber, 0);
    if (!entry) {
      TestRunner.addResult('source line ' + sourceLineNumber + ' has no mappings.');
      return;
    }
    TestRunner.addResult(compiledLineNumber + ' === ' + entry.lineNumber);
    TestRunner.addResult(compiledColumnNumber + ' === ' + entry.columnNumber);
  }

  TestRunner.runTestSuite([
    function testSimpleMap(next) {
      /*
                example.js:
                0         1         2         3
                012345678901234567890123456789012345
                function add(variable_x, variable_y)
                {
                    return variable_x + variable_y;
                }

                var global = "foo";
                ----------------------------------------
                example-compiled.js:
                0         1         2         3
                012345678901234567890123456789012345
                function add(a,b){return a+b}var global="foo";
                foo
            */
      var mappingPayload = {
        'mappings': 'AAASA,QAAAA,IAAG,CAACC,CAAD,CAAaC,CAAb,CACZ,CACI,MAAOD,EAAP,CAAoBC,CADxB,CAIA,IAAIC,OAAS;A',
        'sources': ['example.js']
      };
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);

      checkMapping(0, 9, 'example.js', 0, 9, mapping);
      checkMapping(0, 13, 'example.js', 0, 13, mapping);
      checkMapping(0, 15, 'example.js', 0, 25, mapping);
      checkMapping(0, 18, 'example.js', 2, 4, mapping);
      checkMapping(0, 25, 'example.js', 2, 11, mapping);
      checkMapping(0, 27, 'example.js', 2, 24, mapping);
      checkMapping(1, 0, undefined, undefined, undefined, mapping);

      checkReverseMapping(0, 0, 'example.js', 0, mapping);
      checkReverseMapping(0, 17, 'example.js', 1, mapping);
      checkReverseMapping(0, 18, 'example.js', 2, mapping);
      checkReverseMapping(0, 29, 'example.js', 4, mapping);
      checkReverseMapping(0, 29, 'example.js', 5, mapping);

      next();
    },

    function testNoMappingEntry(next) {
      var mappingPayload = {'mappings': 'AAAA,C,CAAE;', 'sources': ['example.js']};
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);
      checkMapping(0, 0, 'example.js', 0, 0, mapping);
      var entry = mapping.findEntry(0, 1);
      TestRunner.assertTrue(!entry.sourceURL);
      checkMapping(0, 2, 'example.js', 0, 2, mapping);
      next();
    },

    function testEmptyLine(next) {
      var mappingPayload = {'mappings': 'AAAA;;;CACA', 'sources': ['example.js']};
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);
      checkMapping(0, 0, 'example.js', 0, 0, mapping);
      checkReverseMapping(3, 1, 'example.js', 1, mapping);
      next();
    },

    function testNonSortedMappings(next) {
      /*
                example.js:
                ABCD

                compiled.js:
                DCBA
             */
      var mappingPayload = {
        // mappings go in reversed direction.
        'mappings': 'GAAA,DAAC,DAAC,DAAC',
        'sources': ['example.js']
      };
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);
      checkMapping(0, 0, 'example.js', 0, 3, mapping);
      checkMapping(0, 1, 'example.js', 0, 2, mapping);
      checkMapping(0, 2, 'example.js', 0, 1, mapping);
      checkMapping(0, 3, 'example.js', 0, 0, mapping);
      next();
    },

    function testSections(next) {
      var mappingPayload = {
        'sections': [
          {
            'offset': {'line': 0, 'column': 0},
            'map': {'mappings': 'AAAA,CAEC', 'sources': ['source1.js', 'source2.js']}
          },
          {'offset': {'line': 2, 'column': 10}, 'map': {'mappings': 'AAAA,CAEC', 'sources': ['source2.js']}}
        ]
      };
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);
      TestRunner.assertEquals(2, mapping.sourceURLs().length);
      checkMapping(0, 0, 'source1.js', 0, 0, mapping);
      checkMapping(0, 1, 'source1.js', 2, 1, mapping);
      checkMapping(2, 10, 'source2.js', 0, 0, mapping);
      checkMapping(2, 11, 'source2.js', 2, 1, mapping);
      next();
    },

    function testSourceRoot(next) {
      /*
                example.js:
                0         1         2         3
                012345678901234567890123456789012345
                function add(variable_x, variable_y)
                {
                    return variable_x + variable_y;
                }

                var global = "foo";
                ----------------------------------------
                example-compiled.js:
                0         1         2         3
                012345678901234567890123456789012345
                function add(a,b){return a+b}var global="foo";
            */
      var mappingPayload = {
        'mappings': 'AAASA,QAAAA,IAAG,CAACC,CAAD,CAAaC,CAAb,CACZ,CACI,MAAOD,EAAP,CAAoBC,CADxB,CAIA,IAAIC,OAAS;',
        'sources': ['example.js'],
        'sourceRoot': '/'
      };
      var mapping = new SDK.TextSourceMap('compiled.js', 'source-map.json', mappingPayload);
      checkMapping(0, 9, '/example.js', 0, 9, mapping);
      checkReverseMapping(0, 0, '/example.js', 0, mapping);
      next();
    },

    function testNameClash(next) {
      var mappingPayload = {
        'mappings': 'AAASA,QAAAA,IAAG,CAACC,CAAD,CAAaC,CAAb,CACZ,CACI,MAAOD,EAAP,CAAoBC,CADxB,CAIA,IAAIC,OAAS;',
        'sources': ['example.js'],
        'sourcesContent': ['var i = 0;']
      };
      var mapping = new SDK.TextSourceMap('example.js', 'source-map.json', mappingPayload);
      checkMapping(0, 9, 'example.js', 0, 9, mapping);
      checkReverseMapping(0, 0, 'example.js', 0, mapping);
      next();
    },

    function testNameIndexes(next) {
      /*
               ------------------------------------------------------------------------------------
               chrome_issue_611738.clj:
               (ns devtools-sample.chrome-issue-611738)

               (defmacro m []
                 `(let [generated# "value2"]))
               ------------------------------------------------------------------------------------
               chrome_issue_611738.cljs:
               (ns devtools-sample.chrome-issue-611738
               (:require-macros [devtools-sample.chrome-issue-611738 :refer [m]]))

               (let [name1 "value1"]
                 (m))
               ------------------------------------------------------------------------------------
               chrome_issue_611738.js:
               // Compiled by ClojureScript 1.9.89 {}
               goog.provide('devtools_sample.chrome_issue_611738');
               goog.require('cljs.core');
               var name1_31466 = "value1";
               var generated31465_31467 = "value2";

               //# sourceMappingURL=chrome_issue_611738.js.map
               ------------------------------------------------------------------------------------
               chrome_issue_611738.js.map:
               {"version":3,"file":"\/Users\/darwin\/code\/cljs-devtools-sample\/resources\/public\/_compiled\/demo\/devtools_sample\/chrome_issue_611738.js","sources":["chrome_issue_611738.cljs"],"lineCount":7,"mappings":";AAAA;;AAGA,kBAAA,dAAMA;AAAN,AACE,IAAAC,uBAAA;AAAA,AAAA","names":["name1","generated31465"]}
               ------------------------------------------------------------------------------------
             */

      var mappingPayload = {
        'sources': ['chrome_issue_611738.cljs'],
        'mappings': ';AAAA;;AAGA,kBAAA,dAAMA;AAAN,AACE,IAAAC,uBAAA;AAAA,AAAA',
        'names': ['name1', 'generated31465']
      };
      var mapping = new SDK.TextSourceMap('chrome_issue_611738.js', 'chrome_issue_611738.js.map', mappingPayload);
      mapping.mappings().forEach(function(entry) {
        const name = entry.name ? '\'' + entry.name + '\'' : '[no name assigned]';
        TestRunner.addResult(entry.lineNumber + ':' + entry.columnNumber + ' > ' + name);
      });
      next();
    }
  ]);
})();
