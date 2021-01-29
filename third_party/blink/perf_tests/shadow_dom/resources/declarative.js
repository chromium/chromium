(function() {
  function getSnippet(useShadowDom,innerContent) {
    innerContent = innerContent ?? '<span><!--This is the leaf node--></span>';
    let openTag = useShadowDom ? '<template shadowroot=open>' : '<x-elemnt shadowroot=open>';
    let closeTag = useShadowDom ? '</template>' : '</x-elemnt>';
    let hiddenLightDomContent = useShadowDom ? '<span>Some non-slotted light dom content</span>' : '<!--   Some hidden light-dom content here   -->';
    return `<div class="host">${openTag}${innerContent}<span><!--Shadow content here--></span>${closeTag}${hiddenLightDomContent}</div>`;
  }

  function getShadowMarkup(useShadowDom, depth, copies) {
    let snippet = getSnippet(useShadowDom);
    for (let d=1;d<depth;++d) {
      snippet = getSnippet(useShadowDom, snippet);
    }
    let html = '<!DOCTYPE html><body>';
    for(let i=0;i<copies;++i) {
      html += snippet;
    }
    return html;
  }

  const dom_parser = new DOMParser();
  function parseHtml(html) {
    return dom_parser.parseFromString(html, 'text/html', {includeShadowRoots: true});
  }

  function measureParse(html) {
    let start = PerfTestRunner.now();
    parseHtml(html);
    return PerfTestRunner.now() - start;
  }

  function parseAndAppend(parent, html) {
    const fragment = dom_parser.parseFromString(html, 'text/html', {includeShadowRoots: true});
    parent.replaceChildren(...fragment.body.childNodes);
  }

  function measureParseAndAppend(parent, html) {
    parent.replaceChildren(); // Ensure empty
    let start = PerfTestRunner.now();
    parseAndAppend(parent, html);
    return PerfTestRunner.now() - start;
  }

  // Do some double-checks that things are working:
  if (!HTMLTemplateElement.prototype.hasOwnProperty("shadowRoot")) {
    PerfTestRunner.logFatalError('Declarative Shadow DOM not enabled/supported');
  }
  const test_div = document.createElement('div');
  measureParseAndAppend(test_div, getShadowMarkup(true, 1, 1));
  const first_host = test_div.firstChild;
  if (!first_host.shadowRoot) {
    PerfTestRunner.logFatalError('Declarative Shadow DOM not detected');
  }
  if (getShadowMarkup(true, 5, 6).length !== getShadowMarkup(false, 5, 6).length) {
    PerfTestRunner.logFatalError('Shadow and light DOM content should have identical length');
  }

  window.parseHtml = parseHtml;
  window.measureParse = measureParse;
  window.parseAndAppend = parseAndAppend;
  window.measureParseAndAppend = measureParseAndAppend;
  window.getShadowMarkup = getShadowMarkup;
})();