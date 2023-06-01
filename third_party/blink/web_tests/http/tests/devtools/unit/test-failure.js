
import {TestRunner} from 'test_runner';
(async function() {
  TestRunner.addResult("Tests that a test will properly exit if it has an asynchronous error.");
  setTimeout(_ => { throw {stack: "This error is expected"} }, 0);
})();
