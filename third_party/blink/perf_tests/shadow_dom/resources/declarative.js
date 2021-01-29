(function() {
  const lightDomElementName = 'x-elemnt'; // Must have the same length as 'template'
  function getSnippet(useShadowDom,innerContent,lightDomDuplicates) {
    innerContent = innerContent ?? '<span><!--This is the leaf node--></span>';
    lightDomDuplicates = lightDomDuplicates ?? 1;
    PerfTestRunner.assert_true(!useShadowDom || lightDomDuplicates === 1,'Only light dom content can use duplicates');
    let openTag = useShadowDom ? '<template shadowroot=open>' : `<${lightDomElementName} shadowroot=open>`;
    let closeTag = useShadowDom ? '</template>' : `</${lightDomElementName}>`;
    let hiddenLightDomContent = useShadowDom ? '<span>Some non-slotted light dom content</span>' : '<!--   Some hidden light-dom content here   -->';
    let extraCopies = '';
    while (lightDomDuplicates>1) {
      extraCopies += `${openTag}${closeTag}`;
      --lightDomDuplicates;
    }
    return `<div class="host">${extraCopies}${openTag}${innerContent}<span><!--Shadow content here--></span>${closeTag}${hiddenLightDomContent}</div>`;
  }

  function getShadowMarkup(useShadowDom, depth, copies, lightDomDuplicates) {
    let snippet = undefined;
    for (let d=0;d<depth;++d) {
      snippet = getSnippet(useShadowDom, snippet, lightDomDuplicates);
    }
    let html = '<!DOCTYPE html><body>';
    for(let i=0;i<copies;++i) {
      html += snippet;
    }
    return html;
  }

  const domParser = new DOMParser();
  function parseHtml(html) {
    return domParser.parseFromString(html, 'text/html', {includeShadowRoots: true});
  }

  function measureParse(html) {
    let start = PerfTestRunner.now();
    parseHtml(html);
    return PerfTestRunner.now() - start;
  }

  function parseAndAppend(parent, html) {
    const fragment = domParser.parseFromString(html, 'text/html', {includeShadowRoots: true});
    parent.replaceChildren(...fragment.body.childNodes);
  }

  function measureParseAndAppend(parent, html) {
    parent.replaceChildren(); // Ensure empty
    let start = PerfTestRunner.now();
    parseAndAppend(parent, html);
    return PerfTestRunner.now() - start;
  }

  // Do some double-checks that things are working:
  function testParse(html) {
    const test_div = document.createElement('div');
    measureParseAndAppend(test_div, html);
    return test_div;
  }
  PerfTestRunner.assert_true(HTMLTemplateElement.prototype.hasOwnProperty("shadowRoot"),'Declarative Shadow DOM not enabled/supported');
  PerfTestRunner.assert_true(testParse(getShadowMarkup(true, 1, 1)).firstChild.shadowRoot,'Declarative Shadow DOM not detected');
  PerfTestRunner.assert_true(getShadowMarkup(true, 5, 6).length === getShadowMarkup(false, 5, 6).length,'Shadow and light DOM content should have identical length');
  const light1 = testParse(getShadowMarkup(false, 5, 6, /*lightDomDuplicates=*/1)).querySelectorAll(lightDomElementName).length;
  const light2 = testParse(getShadowMarkup(false, 5, 6, /*lightDomDuplicates=*/2)).querySelectorAll(lightDomElementName).length;
  PerfTestRunner.assert_true(light1*2 === light2,"The lightDomDuplicates parameter isn't working");

  window.parseHtml = parseHtml;
  window.measureParse = measureParse;
  window.parseAndAppend = parseAndAppend;
  window.measureParseAndAppend = measureParseAndAppend;
  window.getShadowMarkup = getShadowMarkup;
})();