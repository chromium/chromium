// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`This test checks Web Inspector utilities.\n`);


  TestRunner.runTestSuite([
    function orderedMergeIntersect(next) {
      function comparator(a, b) {
        return a - b;
      }
      function count(a, x) {
        return a.upperBound(x) - a.lowerBound(x);
      }
      function testAll(a, b) {
        testOperation(a, b, a.mergeOrdered(b, comparator), Math.max, 'U');
        testOperation(a, b, a.intersectOrdered(b, comparator), Math.min, 'x');
      }
      function testOperation(a, b, actual, checkOperation, opName) {
        var allValues = a.concat(b).concat(actual);
        for (var i = 0; i < allValues.length; ++i) {
          var value = allValues[i];
          const expectedCount =
              checkOperation(count(a, value), count(b, value));
          const actualCount = count(actual, value);
          TestRunner.assertEquals(
              expectedCount, actualCount,
              'Incorrect result for value: ' + value + ' at [' + a + '] ' + opName + ' [' + b + '] -> [' + actual +
                  ']');
        }
        TestRunner.assertEquals(JSON.stringify(actual.sort()), JSON.stringify(actual), 'result array is ordered');
      }
      var testArrays = [
        [], [], [1], [], [1, 2, 2, 2, 3], [], [4, 5, 5, 8, 8], [1, 1, 1, 2, 6], [1, 2, 2, 2, 2, 3, 3, 4],
        [2, 2, 2, 3, 3, 3, 3], [1, 2, 3, 4, 5], [1, 2, 3]
      ];
      for (var i = 0; i < testArrays.length; i += 2) {
        testAll(testArrays[i], testArrays[i + 1]);
        testAll(testArrays[i + 1], testArrays[i]);
      }
      next();
    },

    function lowerBoundTest(next) {
      var testArrays = [[], [1], [-1, -1, 0, 0, 0, 0, 2, 3, 4, 4, 4, 7, 9, 9, 9]];

      function testArray(array, useComparator) {
        function comparator(a, b) {
          return a < b ? -1 : (a > b ? 1 : 0);
        }

        for (var value = -2; value <= 12; ++value) {
          var index = useComparator ? array.lowerBound(value, comparator) : array.lowerBound(value);
          TestRunner.assertTrue(0 <= index && index <= array.length, 'index is within bounds');
          TestRunner.assertTrue(index === 0 || array[index - 1] < value, 'array[index - 1] < value');
          TestRunner.assertTrue(index === array.length || array[index] >= value, 'array[index] >= value');
        }
      }

      for (var i = 0, l = testArrays.length; i < l; ++i) {
        testArray(testArrays[i], false);
        testArray(testArrays[i], true);
      }
      next();
    },

    function upperBoundTest(next) {
      var testArrays = [[], [1], [-1, -1, 0, 0, 0, 0, 2, 3, 4, 4, 4, 7, 9, 9, 9]];

      function testArray(array, useComparator) {
        function comparator(a, b) {
          return a < b ? -1 : (a > b ? 1 : 0);
        }

        for (var value = -2; value <= 12; ++value) {
          var index = useComparator ? array.upperBound(value, comparator) : array.upperBound(value);
          TestRunner.assertTrue(0 <= index && index <= array.length, 'index is within bounds');
          TestRunner.assertTrue(index === 0 || array[index - 1] <= value, 'array[index - 1] <= value');
          TestRunner.assertTrue(index === array.length || array[index] > value, 'array[index] > value');
        }
      }

      for (var i = 0, l = testArrays.length; i < l; ++i) {
        testArray(testArrays[i], false);
        testArray(testArrays[i], true);
      }
      next();
    },

    function sortRangeTest(next) {
      var testArrays = [[], [1], [2, 1], [6, 4, 2, 7, 10, 15, 1], [10, 44, 3, 6, 56, 66, 10, 55, 32, 56, 2, 5]];

      function testArray(array) {
        function comparator(a, b) {
          return a < b ? -1 : (a > b ? 1 : 0);
        }

        function compareArrays(a, b, message) {
          TestRunner.assertEquals(JSON.stringify(a), JSON.stringify(b), message);
        }

        for (var left = 0, l = array.length - 1; left < l; ++left) {
          for (var right = left, r = array.length; right < r; ++right) {
            for (var first = left; first <= right; ++first) {
              for (var count = 1, k = right - first + 1; count <= k; ++count) {
                var actual = array.slice(0);
                actual.sortRange(comparator, left, right, first, first + count - 1);
                compareArrays(array.slice(0, left), actual.slice(0, left), 'left ' + left + ' ' + right + ' ' + count);
                compareArrays(
                    array.slice(right + 1), actual.slice(right + 1), 'right ' + left + ' ' + right + ' ' + count);
                var middle = array.slice(left, right + 1);
                middle.sort(comparator);
                compareArrays(
                    middle.slice(first - left, first - left + count), actual.slice(first, first + count),
                    'sorted ' + left + ' ' + right + ' ' + first + ' ' + count);
                const actualRest = actual.slice(first + count, right + 1);
                actualRest.sort(comparator);
                compareArrays(
                    middle.slice(first - left + count), actualRest,
                    'unsorted ' + left + ' ' + right + ' ' + first + ' ' + count);
              }
            }
          }
        }
      }

      for (var i = 0, len = testArrays.length; i < len; ++i)
        testArray(testArrays[i]);
      next();
    },

    function naturalOrderComparatorTest(next) {
      var testArray = [
        'dup', 'a1',   'a4222',  'a91',       'a07',      'dup', 'a7',        'a007',      'abc00',     'abc0',
        'abc', 'abcd', 'abc000', 'x10y20z30', 'x9y19z29', 'dup', 'x09y19z29', 'x10y22z23', 'x10y19z43', '1',
        '10',  '11',   'dup',    '2',         '2',        '2',   '555555',    '5',         '5555',      'dup',
      ];

      for (var i = 0, n = testArray.length; i < n; ++i)
        TestRunner.assertEquals(
            0, String.naturalOrderComparator(testArray[i], testArray[i]), 'comparing equal strings');

      testArray.sort(String.naturalOrderComparator);
      TestRunner.addResult('Sorted in natural order: [' + testArray.join(', ') + ']');

      // Check comparator's transitivity.
      for (var i = 0, n = testArray.length; i < n; ++i) {
        for (var j = 0; j < n; ++j) {
          var a = testArray[i];
          var b = testArray[j];
          var diff = String.naturalOrderComparator(a, b);
          if (diff === 0)
            TestRunner.assertEquals(a, b, 'zero diff');
          else if (diff < 0)
            TestRunner.assertTrue(i < j);
          else
            TestRunner.assertTrue(i > j);
        }
      }
      next();
    },

    function stringHashTest(next) {
      var stringA = ' '.repeat(10000);
      var stringB = stringA + ' ';
      var hashA = String.hashCode(stringA);
      TestRunner.assertTrue(hashA !== String.hashCode(stringB));
      TestRunner.assertTrue(isFinite(hashA));
      TestRunner.assertTrue(hashA + 1 !== hashA);
      next();
    },

    function trimMiddle(next) {
      var testArray = [
        '', '!', '\uD83D\uDE48A\uD83D\uDE48L\uD83D\uDE48I\uD83D\uDE48N\uD83D\uDE48A\uD83D\uDE48\uD83D\uDE48', 'test'
      ];
      for (var string of testArray) {
        for (var maxLength = string.length + 1; maxLength > 0; --maxLength) {
          var trimmed = string.trimMiddle(maxLength);
          TestRunner.addResult(trimmed);
          TestRunner.assertTrue(trimmed.length <= maxLength);
        }
      }
      next();
    }
  ]);
})();
