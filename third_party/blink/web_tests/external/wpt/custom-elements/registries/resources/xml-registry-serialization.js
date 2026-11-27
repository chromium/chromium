'use strict';

const xmlRegistryHTMLNamespace = 'http://www.w3.org/1999/xhtml';

function xmlRegistryElement(localName, registry = customElements) {
  return document.createElementNS(
      xmlRegistryHTMLNamespace, localName, {customElementRegistry: registry});
}

function xmlRegistrySerialize(node) {
  return new XMLSerializer().serializeToString(node);
}

function xmlRegistryParse(markup) {
  return new DOMParser().parseFromString(markup, 'application/xml').documentElement;
}

for (const kind of ['global', 'null', 'scoped']) {
  test(() => {
    const registry = kind === 'global' ? customElements :
        kind === 'null' ? null : new CustomElementRegistry();
    const element = xmlRegistryElement('section', registry);
    const parsed = xmlRegistryParse(xmlRegistrySerialize(element));
    assert_equals(parsed.hasAttribute('customelementregistry'), kind !== 'global');
    assert_equals(element.attributes.length, 0);
    assert_equals(element.customElementRegistry, registry);
  }, `XMLSerializer uses the document context for a detached ${kind} registry element`);
}

for (const [parentKind, childKind, expectedMarker] of [
  ['global', 'null', true],
  ['global', 'scoped', true],
  ['null', 'null', false],
  ['scoped', 'scoped', false],
  ['scoped', 'other', true],
  ['null', 'scoped', true],
  ['scoped', 'null', true],
  ['null', 'global', false],
  ['scoped', 'global', false],
]) {
  test(() => {
    const registries = {
      global: customElements,
      null: null,
      scoped: new CustomElementRegistry(),
      other: new CustomElementRegistry(),
    };
    const parent = xmlRegistryElement('section', registries[parentKind]);
    parent.append(xmlRegistryElement('span', registries[childKind]));
    const parsed = xmlRegistryParse(xmlRegistrySerialize(parent));
    assert_equals(
        parsed.firstElementChild.hasAttribute('customelementregistry'),
        expectedMarker);
  }, `XMLSerializer handles a ${childKind} registry child of a ${parentKind} registry parent`);
}

test(() => {
  const registry = new CustomElementRegistry();
  const parent = xmlRegistryElement('section', registry);
  const inheriting = xmlRegistryElement('span', registry);
  const differing = xmlRegistryElement('span', null);
  parent.append(inheriting, differing);
  assert_false(xmlRegistryParse(xmlRegistrySerialize(inheriting)).hasAttribute('customelementregistry'));
  assert_true(xmlRegistryParse(xmlRegistrySerialize(differing)).hasAttribute('customelementregistry'));
}, 'XMLSerializer compares an element with its parent registry, like outerHTML');

test(() => {
  const parent = xmlRegistryElement('section');
  parent.append(
      xmlRegistryElement('span', null),
      xmlRegistryElement('span'),
      xmlRegistryElement('span', new CustomElementRegistry()),
      xmlRegistryElement('span', null));
  const parsed = xmlRegistryParse(xmlRegistrySerialize(parent));
  assert_array_equals(
      Array.from(parsed.children, child => child.hasAttribute('customelementregistry')),
      [true, false, true, true]);
}, 'XML serialization keeps each sibling registry context independent');

test(() => {
  const element = xmlRegistryElement('section', new CustomElementRegistry());
  element.setAttribute('customelementregistry', 'named');
  const parsed = xmlRegistryParse(xmlRegistrySerialize(element));
  assert_equals(parsed.getAttribute('customelementregistry'), 'named');
  assert_equals(
      Array.from(parsed.attributes).filter(
          attribute => attribute.localName === 'customelementregistry').length,
      1);
  assert_equals(element.getAttribute('customelementregistry'), 'named');
}, 'XML serialization preserves an existing registry attribute without duplicating it');

test(() => {
  const element = xmlRegistryElement('section', new CustomElementRegistry());
  element.setAttributeNS('urn:example', 'example:customelementregistry', 'named');
  const parsed = xmlRegistryParse(xmlRegistrySerialize(element));
  assert_equals(parsed.getAttributeNS(null, 'customelementregistry'), '');
  assert_equals(parsed.getAttributeNS('urn:example', 'customelementregistry'), 'named');
  assert_false(element.hasAttributeNS(null, 'customelementregistry'));
}, 'A namespaced attribute does not suppress the generated XML registry attribute');

test(() => {
  const svgNamespace = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(svgNamespace, 'svg', {customElementRegistry: null});
  svg.append(document.createElementNS(
      svgNamespace, 'g', {customElementRegistry: new CustomElementRegistry()}));
  const parsed = xmlRegistryParse(xmlRegistrySerialize(svg));
  assert_equals(parsed.namespaceURI, svgNamespace);
  assert_false(parsed.hasAttribute('customelementregistry'));
  assert_false(parsed.firstElementChild.hasAttribute('customelementregistry'));
}, 'XML serialization does not generate registry attributes on foreign elements');

test(() => {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.append(xmlRegistryElement('span', null));
  const parsed = xmlRegistryParse(xmlRegistrySerialize(svg));
  assert_equals(parsed.firstElementChild.namespaceURI, xmlRegistryHTMLNamespace);
  assert_true(parsed.firstElementChild.hasAttribute('customelementregistry'));
}, 'XML serialization detects an HTML registry boundary inside a foreign element');

test(() => {
  const template = xmlRegistryElement('template');
  template.innerHTML = `<span xmlns="${xmlRegistryHTMLNamespace}"></span>`;
  assert_equals(template.content.firstElementChild.customElementRegistry, null);
  assert_false(xmlRegistrySerialize(template).includes('customelementregistry'));
}, 'XML serialization uses the template contents registry for descendants');

test(() => {
  const registry = new CustomElementRegistry();
  const source = xmlRegistryElement('section', registry);
  source.append(xmlRegistryElement('span', registry));
  const xmlDocument = document.implementation.createDocument(null, 'root');
  xmlDocument.replaceChild(source, xmlDocument.documentElement);
  const parsed = xmlRegistryParse(xmlRegistrySerialize(xmlDocument));
  assert_true(parsed.hasAttribute('customelementregistry'));
  assert_false(parsed.firstElementChild.hasAttribute('customelementregistry'));
}, 'XML document serialization propagates registry context from the document root');

test(() => {
  const fragment = document.createDocumentFragment();
  fragment.append(
      xmlRegistryElement('section', null),
      xmlRegistryElement('section', new CustomElementRegistry()),
      xmlRegistryElement('section'));
  const parsed = xmlRegistryParse(`<root>${xmlRegistrySerialize(fragment)}</root>`);
  assert_array_equals(
      Array.from(parsed.children, child => child.hasAttribute('customelementregistry')),
      [true, true, false]);
}, 'XMLSerializer uses the document context for top-level fragment children');

test(() => {
  const registry = new CustomElementRegistry();
  const shadowRoot = xmlRegistryElement('div').attachShadow(
      {mode: 'open', customElementRegistry: registry});
  shadowRoot.append(xmlRegistryElement('span', registry), xmlRegistryElement('span', null));
  const parsed = xmlRegistryParse(`<root>${xmlRegistrySerialize(shadowRoot)}</root>`);
  assert_array_equals(
      Array.from(parsed.children, child => child.hasAttribute('customelementregistry')),
      [false, true]);
}, 'XMLSerializer uses the shadow root registry context for shadow root children');

test(() => {
  const registry = new CustomElementRegistry();
  const source = xmlRegistryElement('section', registry);
  source.append(xmlRegistryElement('span', registry));
  const target = xmlRegistryElement('div');
  target.innerHTML = xmlRegistrySerialize(source);
  const parsed = target.firstElementChild;
  assert_equals(parsed.customElementRegistry, null);
  assert_equals(parsed.firstElementChild.customElementRegistry, null);
  registry.initialize(parsed);
  assert_equals(parsed.customElementRegistry, registry);
  assert_equals(parsed.firstElementChild.customElementRegistry, registry);
  assert_false(source.hasAttribute('customelementregistry'));
}, 'An XML-serialized scoped registry boundary reparses as null and can be initialized');
