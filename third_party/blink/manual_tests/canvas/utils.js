// utils.js
(function(window, undefined) {

const COLD_TIME = 2.0;
const MAX_WINDOW_TIME = 20.0;
const BEST_CONFIDENCE = 0.0005;

const ZSTAR = {
  0.80: 1.28,
  0.90: 1.645,
  0.95: 1.96,
  0.98: 2.33,
  0.99: 2.58,
  0.999: 3.10,
  0.9998: 3.49,
}

const perf = window.perf = {};
let timers = perf.timers = [];
// Time the experiment started. Reset after warming up.
perf.firstTime = performance.now()/1000.0;
perf.lastTime = perf.firstTime;
// Time to wait until warm up is done.
perf.coldTime = perf.firstTime + COLD_TIME;

let finished = false;

// window.setImmediate allows us to run at fps higher than 60
if (window.setImmediate === undefined) {
    let _immediateIdCounter = 1;
    const _immediateFuncs = {};

    window.setImmediate = (func) => {
        const index = _immediateIdCounter++;
        _immediateFuncs[index] = func;
        window.postMessage("immediate trigger:" + index, "*");
        return index;
    };

    window.clearImmediate = (id) => {
        _immediateFuncs[id];
    };

    window.addEventListener("message", (event) => {
        if ((typeof event.data !== "string") ||
            (event.data.indexOf("immediate trigger:") !== 0)) {
            return;
        }

        const index = event.data.slice("immediate trigger:".length);

        const callback = _immediateFuncs[index];
        if (callback === undefined) {
            return;
        }

        delete _immediateFuncs[index];

        callback();
    });
}

// Overwrite this function so that tests run at the maximum possible fps
requestAnimationFrame = window.setImmediate;

perf.ntos = function(n) {
  let unit = "s";
  if (n < 1) {
    unit = "ms";
    n *= 1000;
  }
  const p = Math.floor(n);
  let r = "" + Math.round((n-p)*100);
  while (r.length < 2) r = "0" + r;
  return p + "." + r + unit;
}

perf.ntoserr = function(avg, std, for_console) {
  let unit = "s";
  if (avg < 1) {
    unit = "ms";
    avg *= 1000;
    std *= 1000;
  }
  const p = Math.floor(avg);
  let r = "" + Math.round((avg-p)*100);
  while (r.length < 2) r = "0" + r;

  const p2 = Math.floor(std);
  let r2 = "" + Math.round((std-p2)*100);
  while (r2.length < 2) r2 = "0" + r2;

  const plus = for_console ? "±" : "&plusmn;";
  return p + "." + r + plus + p2 + "." + r2 + unit;
}

function getExperimentName() {
  let name = window.location.pathname.split('/');
  name = name[name.length-1].split('.')[0];

  if (window.location.search == "?scroll") {
    name += " scroll";
  }

  return name;
}

function SET(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}

function GET(key, def=null) {
  const v = localStorage.getItem(key);
  if (v === null) return def;
  return JSON.parse(v);
}

function runNextPerfTest() {
  const scripts = GET("scripts", []);
  const next = scripts.shift();
  if (!next) return;
  SET("scripts", scripts);
  console.log(next);
  window.location.href = next;
}

function done() {
  if (finished) return;
  finished = true;
  let [average, conf] = perf.stats();

  let res = GET("perf", []);

  const name = getExperimentName();
  res = res.filter(x => x[0] != name);
  res.push([ name, average, conf ]);
  SET("perf", res);
  runNextPerfTest();
}

perf.onContinuousMode = function() {
  const s = GET("scripts");
  return !!(s && s.length);
}

perf.stats = function() {
  let average = 0.0;
  for (let i = 0; i < timers.length; ++i) {
    average += timers[i];
  }
  average = average/timers.length;
  let stddev = 0.0;
  for (let i = 0; i < timers.length; ++i) {
    stddev += Math.pow(timers[i] - average, 2);
  }
  stddev = Math.sqrt(stddev/timers.length);

  const conf = ZSTAR[0.999] * stddev / Math.sqrt(timers.length);

  return [average, conf];
}

let consoleTime = 0.0;
perf.measure = function() {
  const t = performance.now()/1000.0;
  const delta = t - perf.lastTime;
  perf.lastTime = t;

  if (t < perf.coldTime) {
    if (perf.onContinuousMode()) {
      consoleTime -= delta;
      if (consoleTime <= 0.0) {
        consoleTime += 5.0;
        const left = perf.coldTime - perf.lastTime;
        console.log(`warming up [${perf.ntos(left)}]`);
      }
    }
    perf.firstTime = perf.lastTime = t;
    window.setImmediate(perf.measure);
    return;
  }

  timers.push(delta);

  if (perf.onContinuousMode()) {
    const time = perf.lastTime - perf.firstTime;
    if (time > MAX_WINDOW_TIME) {
      done();
    } else {
      let [average, conf] = perf.stats();
      if (time > 5.0 && conf < BEST_CONFIDENCE) done();
    }
    consoleTime -= delta;
    if (consoleTime <= 0.0) {
      consoleTime += 5.0;
      let [average, conf] = perf.stats();
      const t = perf.lastTime - perf.firstTime;
      console.log(`avg: ${perf.ntoserr(average, conf, true)} [${perf.ntos(t)}]`);
    }
  }

  window.setImmediate(perf.measure);
}

perf.getResults = function() {
  return GET("perf", []);
}

perf.saveBaseline = function() {
  const res = perf.getResults();
  const ret = {};
  for (let i = 0; i < res.length; ++i) {
    const p = res[i];
    ret[p[0]] = [p[1], p[2]];
  }
  SET("baseline", ret);
}

perf.getBaseline = function() {
  return GET("baseline", {});
}

perf.setupTests = function(tests) {
  SET("scripts", tests);
  SET("perf", []);
  runNextPerfTest();
};

if (performance.memory.usedJSHeapSize == 10000000) {
  console.error("Start Chrome with --disable-frame-rate-limit --enable-precise-memory-info for this test");
}

})(window);
