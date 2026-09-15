const SHADOW_COUNT = 100;
const UPDATES_PER_RUN = 40;

function createShadowTrees(mixinUserCount) {
  const targets = [];
  for (let i = 0; i < SHADOW_COUNT; ++i) {
    const host = document.createElement('div');
    const root = host.attachShadow({mode: 'open'});
    const declaration =
        i < mixinUserCount ? '@apply --perf-mixin;' : 'color: black;';
    root.innerHTML = `
      <style>
        .target {
          ${declaration}
        }
      </style>
      <div class="target">target</div>
    `;
    document.body.append(host);
    targets.push(root.querySelector('.target'));
  }
  PerfTestRunner.forceLayout();
  return targets;
}

function measureMixinInvalidation({description, mixinUserCount, mutate}) {
  const targets = createShadowTrees(mixinUserCount);
  PerfTestRunner.assert_true(
      getComputedStyle(targets[0]).color === 'rgb(255, 0, 0)',
      'The first shadow tree must apply the parent mixin.');
  let useGreen = false;

  PerfTestRunner.measureTime({
    description,
    iterationCount: 20,
    run: () => {
      for (let i = 0; i < UPDATES_PER_RUN; ++i) {
        useGreen = !useGreen;
        mutate(useGreen);
        getComputedStyle(targets[0]).color;
      }
    },
    tracingCategories: 'blink',
    traceEventsToMeasure: [
      'StyleSheetCollection::updateMixins',
      'StyleSheetContents::rebuildRuleSetForMixins',
      'Document::recalcStyle',
    ],
  });
}
