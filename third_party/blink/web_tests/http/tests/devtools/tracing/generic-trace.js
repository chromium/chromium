// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks DevTools timeline is capable of reading and displaying generic traces.\n`);
  Root.Runtime.experiments.enableForTest('timelineShowAllEvents');
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  const rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': 'metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': 'metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {'pid':254229,'tid':0,'ts':0,'ph':'M','cat':'metadata','name':'num_cpus','args':{'number':40}},
    {'pid':254229,'tid':254229,'ts':0,'ph':'M','cat':'metadata','name':'process_sort_index','args':{'sort_index':-6}},
    {'pid':254229,'tid':254229,'ts':0,'ph':'M','cat':'metadata','name':'process_name','args':{'name':'Browser'}},
    {'pid':254229,'tid':254229,'ts':0,'ph':'M','cat':'metadata','name':'process_uptime_seconds','args':{'uptime':2056}},
    {'pid':254229,'tid':254229,'ts':0,'ph':'M','cat':'metadata','name':'thread_name','args':{'name':'CrBrowserMain'}},
    {'pid':254229,'tid':254262,'ts':0,'ph':'M','cat':'metadata','name':'thread_name','args':{'name':'CompositorTileWorker1/254262'}},
    {'pid':254229,'tid':254245,'ts':0,'ph':'M','cat':'metadata','name':'thread_name','args':{'name':'Chrome_IOThread'}},
    {"pid":254229,"tid":254229,"ts":101020,"ph":"X","cat":"toplevel","name":"MessageLoop::RunTask","args":{"src_file":"../../mojo/public/cpp/system/simple_watcher.cc","src_func":"Notify"},"dur":470},
    {"pid":254229,"tid":254262,"ts":101330,"ph":"X","cat":"toplevel","name":"MessageLoop::RunTask","args":{"src_file":"../../mojo/public/cpp/system/simple_watcher.cc","src_func":"Notify"},"dur":440},
    {"pid":254229,"tid":254245,"ts":101640,"ph":"X","cat":"toplevel","name":"MessageLoop::RunTask","args":{"src_file":"../../mojo/public/cpp/system/simple_watcher.cc","src_func":"Notify"},"dur":400},
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'EvaluateScript',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 101000,
      'dur': 10000,
      'args': {'data': {'url': 'https://www.google.com', 'lineNumber': 1337}}
    }
  ];

  const timeline = UI.panels.timeline;
  const model = await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents);
  timeline.setModel(model);

  TestRunner.addResult(`isGenericTrace: ${model.timelineModel().isGenericTrace()}\n`);
  const timelineData = timeline.flameChart.mainDataProvider.timelineData();
  const groups = timelineData.groups;
  groups.forEach((group, index) => {
    TestRunner.addResult(`${index}: ${group.name} ${group.startLevel}`);
  });

  TestRunner.completeTest();
})();
