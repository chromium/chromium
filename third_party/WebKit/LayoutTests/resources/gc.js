// If there is no window.gc() already defined, define one using the best
// method we can find.
// The slow fallback should not hit in the actual test environment.
if (!window.gc)
{
    window.gc = function()
    {
        if (window.GCController)
            return GCController.collectAll();
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 10000; i++)
            gcRec(10);
    }
}

function checkForGCController() {
  if (typeof GCController === "undefined")
    throw "Specific GC operations only supported when running with " +
          "--expose-gc in V8."
}

// With Oilpan tests that rely on garbage collection need to go through
// the event loop in order to get precise garbage collections. Oilpan
// uses conservative stack scanning when not at the event loop and that
// can artificially keep objects alive. Therefore, tests that need to check
// that something is dead need to use this asynchronous collectGarbage
// function.
function asyncGC(callback) {
  checkForGCController();

  if (!callback) {
    return new Promise(resolve => asyncGC(resolve));
  }
  const documentsBefore = internals.numberOfLiveDocuments();
  GCController.asyncCollectAll(function () {
    const documentsAfter = internals.numberOfLiveDocuments();
    if (documentsAfter < documentsBefore) {
      asyncGC(callback);
    } else {
      callback();
    }
  });
}

function asyncMinorGC(callback) {
  checkForGCController();

  GCController.minorCollect();
  // FIXME: we need a better way of waiting for chromium events to happen.
  setTimeout(callback, 0);
}
