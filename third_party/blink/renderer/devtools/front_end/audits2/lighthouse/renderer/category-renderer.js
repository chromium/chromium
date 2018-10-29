/**
 * @license
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

/* globals self, Util */

/** @typedef {import('./dom.js')} DOM */
/** @typedef {import('./report-renderer.js')} ReportRenderer */
/** @typedef {import('./details-renderer.js')} DetailsRenderer */
/** @typedef {import('./util.js')} Util */

class CategoryRenderer {
  /**
   * @param {DOM} dom
   * @param {DetailsRenderer} detailsRenderer
   */
  constructor(dom, detailsRenderer) {
    /** @type {DOM} */
    this.dom = dom;
    /** @type {DetailsRenderer} */
    this.detailsRenderer = detailsRenderer;
    /** @type {ParentNode} */
    this.templateContext = this.dom.document();

    this.detailsRenderer.setTemplateContext(this.templateContext);
  }

  /**
   * @param {LH.ReportResult.AuditRef} audit
   * @param {number} index
   * @return {Element}
   */
  renderAudit(audit, index) {
    const tmpl = this.dom.cloneTemplate('#tmpl-lh-audit', this.templateContext);
    return this.populateAuditValues(audit, index, tmpl);
  }

  /**
   * Populate an DOM tree with audit details. Used by renderAudit and renderOpportunity
   * @param {LH.ReportResult.AuditRef} audit
   * @param {number} index
   * @param {DocumentFragment} tmpl
   * @return {Element}
   */
  populateAuditValues(audit, index, tmpl) {
    const auditEl = this.dom.find('.lh-audit', tmpl);
    auditEl.id = audit.result.id;
    const scoreDisplayMode = audit.result.scoreDisplayMode;

    if (audit.result.displayValue) {
      const displayValue = Util.formatDisplayValue(audit.result.displayValue);
      this.dom.find('.lh-audit__display-text', auditEl).textContent = displayValue;
    }

    const titleEl = this.dom.find('.lh-audit__title', auditEl);
    titleEl.appendChild(this.dom.convertMarkdownCodeSnippets(audit.result.title));
    this.dom.find('.lh-audit__description', auditEl)
        .appendChild(this.dom.convertMarkdownLinkSnippets(audit.result.description));

    const header = /** @type {HTMLDetailsElement} */ (this.dom.find('details', auditEl));
    if (audit.result.details && audit.result.details.type) {
      const elem = this.detailsRenderer.render(audit.result.details);
      elem.classList.add('lh-details');
      header.appendChild(elem);
    }
    this.dom.find('.lh-audit__index', auditEl).textContent = `${index + 1}`;

    // Add chevron SVG to the end of the summary
    this.dom.find('.lh-chevron-container', auditEl).appendChild(this._createChevron());
    this._setRatingClass(auditEl, audit.result.score, scoreDisplayMode);

    if (audit.result.scoreDisplayMode === 'error') {
      auditEl.classList.add(`lh-audit--error`);
      const textEl = this.dom.find('.lh-audit__display-text', auditEl);
      textEl.textContent = Util.UIStrings.errorLabel;
      textEl.classList.add('tooltip-boundary');
      const tooltip = this.dom.createChildOf(textEl, 'div', 'tooltip tooltip--error');
      tooltip.textContent = audit.result.errorMessage || Util.UIStrings.errorMissingAuditInfo;
    } else if (audit.result.explanation) {
      const explEl = this.dom.createChildOf(titleEl, 'div', 'lh-audit-explanation');
      explEl.textContent = audit.result.explanation;
    }
    const warnings = audit.result.warnings;
    if (!warnings || warnings.length === 0) return auditEl;

    // Add list of warnings or singular warning
    const warningsEl = this.dom.createChildOf(titleEl, 'div', 'lh-warnings');
    if (warnings.length === 1) {
      warningsEl.textContent = `${Util.UIStrings.warningHeader} ${warnings.join('')}`;
    } else {
      warningsEl.textContent = Util.UIStrings.warningHeader;
      const warningsUl = this.dom.createChildOf(warningsEl, 'ul');
      for (const warning of warnings) {
        const item = this.dom.createChildOf(warningsUl, 'li');
        item.textContent = warning;
      }
    }
    return auditEl;
  }

  /**
   * @return {HTMLElement}
   */
  _createChevron() {
    const chevronTmpl = this.dom.cloneTemplate('#tmpl-lh-chevron', this.templateContext);
    const chevronEl = this.dom.find('.lh-chevron', chevronTmpl);
    return chevronEl;
  }

  /**
   * @param {Element} element DOM node to populate with values.
   * @param {number|null} score
   * @param {string} scoreDisplayMode
   * @return {Element}
   */
  _setRatingClass(element, score, scoreDisplayMode) {
    const rating = Util.calculateRating(score, scoreDisplayMode);
    element.classList.add(`lh-audit--${rating}`, `lh-audit--${scoreDisplayMode}`);
    return element;
  }

  /**
   * @param {LH.ReportResult.Category} category
   * @return {Element}
   */
  renderCategoryHeader(category) {
    const tmpl = this.dom.cloneTemplate('#tmpl-lh-category-header', this.templateContext);

    const gaugeContainerEl = this.dom.find('.lh-score__gauge', tmpl);
    const gaugeEl = this.renderScoreGauge(category);
    gaugeContainerEl.appendChild(gaugeEl);

    this.dom.find('.lh-category-header__title', tmpl).appendChild(
      this.dom.convertMarkdownCodeSnippets(category.title));
    if (category.description) {
      const descEl = this.dom.convertMarkdownLinkSnippets(category.description);
      this.dom.find('.lh-category-header__description', tmpl).appendChild(descEl);
    }

    return /** @type {Element} */ (tmpl.firstElementChild);
  }

  /**
   * Renders the group container for a group of audits. Individual audit elements can be added
   * directly to the returned element.
   * @param {LH.Result.ReportGroup} group
   * @param {{expandable: boolean, itemCount?: number}} opts
   * @return {Element}
   */
  renderAuditGroup(group, opts) {
    const expandable = opts.expandable;
    const groupEl = this.dom.createElement(expandable ? 'details' : 'div', 'lh-audit-group');
    const summmaryEl = this.dom.createChildOf(groupEl, 'summary', 'lh-audit-group__summary');
    const headerEl = this.dom.createChildOf(summmaryEl, 'div', 'lh-audit-group__header');
    const itemCountEl = this.dom.createChildOf(summmaryEl, 'div', 'lh-audit-group__itemcount');
    if (expandable) {
      const chevronEl = summmaryEl.appendChild(this._createChevron());
      chevronEl.title = Util.UIStrings.auditGroupExpandTooltip;
    }

    if (group.description) {
      const auditGroupDescription = this.dom.createElement('div', 'lh-audit-group__description');
      auditGroupDescription.appendChild(this.dom.convertMarkdownLinkSnippets(group.description));
      groupEl.appendChild(auditGroupDescription);
    }
    headerEl.textContent = group.title;

    if (opts.itemCount) {
      // TODO(i18n): support multiple locales here
      itemCountEl.textContent = `${opts.itemCount} audits`;
    }
    return groupEl;
  }

  /**
   * Find the total number of audits contained within a section.
   * Accounts for nested subsections like Accessibility.
   * @param {Array<Element>} elements
   * @return {number}
   */
  _getTotalAuditsLength(elements) {
    // Create a scratch element to append sections to so we can reuse querySelectorAll().
    const scratch = this.dom.createElement('div');
    elements.forEach(function(element) {
      scratch.appendChild(element);
    });
    const subAudits = scratch.querySelectorAll('.lh-audit');
    if (subAudits.length) {
      return subAudits.length;
    } else {
      return elements.length;
    }
  }

  /**
   * @param {Array<Element>} elements
   * @return {Element}
   */
  _renderFailedAuditsSection(elements) {
    const failedElem = this.dom.createElement('div');
    failedElem.classList.add('lh-failed-audits');
    elements.forEach(elem => failedElem.appendChild(elem));
    return failedElem;
  }

  /**
   * @param {Array<Element>} elements
   * @return {Element}
   */
  renderPassedAuditsSection(elements) {
    const passedElem = this.renderAuditGroup({
      title: Util.UIStrings.passedAuditsGroupTitle,
    }, {expandable: true, itemCount: this._getTotalAuditsLength(elements)});
    passedElem.classList.add('lh-passed-audits');
    elements.forEach(elem => passedElem.appendChild(elem));
    return passedElem;
  }

  /**
   * @param {Array<Element>} elements
   * @return {Element}
   */
  _renderNotApplicableAuditsSection(elements) {
    const notApplicableElem = this.renderAuditGroup({
      title: Util.UIStrings.notApplicableAuditsGroupTitle,
    }, {expandable: true, itemCount: this._getTotalAuditsLength(elements)});
    notApplicableElem.classList.add('lh-audit-group--not-applicable');
    elements.forEach(elem => notApplicableElem.appendChild(elem));
    return notApplicableElem;
  }

  /**
   * @param {Array<LH.ReportResult.AuditRef>} manualAudits
   * @param {string} [manualDescription]
   * @return {Element}
   */
  _renderManualAudits(manualAudits, manualDescription) {
    const group = {title: Util.UIStrings.manualAuditsGroupTitle, description: manualDescription};
    const auditGroupElem = this.renderAuditGroup(group,
        {expandable: true, itemCount: manualAudits.length});
    auditGroupElem.classList.add('lh-audit-group--manual');
    manualAudits.forEach((audit, i) => {
      auditGroupElem.appendChild(this.renderAudit(audit, i));
    });
    return auditGroupElem;
  }

  /**
   * @param {ParentNode} context
   */
  setTemplateContext(context) {
    this.templateContext = context;
    this.detailsRenderer.setTemplateContext(context);
  }

  /**
   * @param {LH.ReportResult.Category} category
   * @return {DocumentFragment}
   */
  renderScoreGauge(category) {
    const tmpl = this.dom.cloneTemplate('#tmpl-lh-gauge', this.templateContext);
    const wrapper = /** @type {HTMLAnchorElement} */ (this.dom.find('.lh-gauge__wrapper', tmpl));
    wrapper.href = `#${category.id}`;
    wrapper.classList.add(`lh-gauge__wrapper--${Util.calculateRating(category.score)}`);

    // Cast `null` to 0
    const numericScore = Number(category.score);
    const gauge = this.dom.find('.lh-gauge', tmpl);
    // 329 is ~= 2 * Math.PI * gauge radius (53)
    // https://codepen.io/xgad/post/svg-radial-progress-meters
    // score of 50: `stroke-dasharray: 164.5 329`;
    /** @type {?SVGCircleElement} */
    const gaugeArc = gauge.querySelector('.lh-gauge-arc');
    if (gaugeArc) {
      gaugeArc.style.strokeDasharray = `${numericScore * 329} 329`;
    }

    const scoreOutOf100 = Math.round(numericScore * 100);
    const percentageEl = this.dom.find('.lh-gauge__percentage', tmpl);
    percentageEl.textContent = scoreOutOf100.toString();
    if (category.score === null) {
      percentageEl.textContent = '?';
      percentageEl.title = Util.UIStrings.errorLabel;
    }

    this.dom.find('.lh-gauge__label', tmpl).textContent = category.title;
    return tmpl;
  }

  /**
   * @param {LH.ReportResult.Category} category
   * @param {Object<string, LH.Result.ReportGroup>} [groupDefinitions]
   * @return {Element}
   */
  render(category, groupDefinitions) {
    const element = this.dom.createElement('div', 'lh-category');
    this.createPermalinkSpan(element, category.id);
    element.appendChild(this.renderCategoryHeader(category));

    const auditRefs = category.auditRefs;
    const manualAudits = auditRefs.filter(audit => audit.result.scoreDisplayMode === 'manual');
    const nonManualAudits = auditRefs.filter(audit => !manualAudits.includes(audit));

    /** @type {Object<string, {passed: Array<LH.ReportResult.AuditRef>, failed: Array<LH.ReportResult.AuditRef>, notApplicable: Array<LH.ReportResult.AuditRef>}>} */
    const auditsGroupedByGroup = {};
    const auditsUngrouped = {passed: [], failed: [], notApplicable: []};

    nonManualAudits.forEach(auditRef => {
      let group;

      if (auditRef.group) {
        const groupId = auditRef.group;

        if (auditsGroupedByGroup[groupId]) {
          group = auditsGroupedByGroup[groupId];
        } else {
          group = {passed: [], failed: [], notApplicable: []};
          auditsGroupedByGroup[groupId] = group;
        }
      } else {
        group = auditsUngrouped;
      }

      if (auditRef.result.scoreDisplayMode === 'not-applicable') {
        group.notApplicable.push(auditRef);
      } else if (Util.showAsPassed(auditRef.result)) {
        group.passed.push(auditRef);
      } else {
        group.failed.push(auditRef);
      }
    });

    const failedElements = /** @type {Array<Element>} */ ([]);
    const passedElements = /** @type {Array<Element>} */ ([]);
    const notApplicableElements = /** @type {Array<Element>} */ ([]);

    auditsUngrouped.failed.forEach((audit, i) => failedElements.push(this.renderAudit(audit, i)));
    auditsUngrouped.passed.forEach((audit, i) => passedElements.push(this.renderAudit(audit, i)));
    auditsUngrouped.notApplicable.forEach((audit, i) => notApplicableElements.push(
        this.renderAudit(audit, i)));

    Object.keys(auditsGroupedByGroup).forEach(groupId => {
      if (!groupDefinitions) return; // We never reach here if there aren't groups, but TSC needs convincing

      const group = groupDefinitions[groupId];
      const groups = auditsGroupedByGroup[groupId];

      if (groups.failed.length) {
        const auditGroupElem = this.renderAuditGroup(group, {expandable: false});
        groups.failed.forEach((item, i) => auditGroupElem.appendChild(this.renderAudit(item, i)));
        auditGroupElem.classList.add('lh-audit-group--unadorned');
        failedElements.push(auditGroupElem);
      }

      if (groups.passed.length) {
        const auditGroupElem = this.renderAuditGroup(group, {expandable: true});
        groups.passed.forEach((item, i) => auditGroupElem.appendChild(this.renderAudit(item, i)));
        auditGroupElem.classList.add('lh-audit-group--unadorned');
        passedElements.push(auditGroupElem);
      }

      if (groups.notApplicable.length) {
        const auditGroupElem = this.renderAuditGroup(group, {expandable: true});
        groups.notApplicable.forEach((item, i) =>
            auditGroupElem.appendChild(this.renderAudit(item, i)));
        auditGroupElem.classList.add('lh-audit-group--unadorned');
        notApplicableElements.push(auditGroupElem);
      }
    });

    if (failedElements.length) {
      const failedElem = this._renderFailedAuditsSection(failedElements);
      element.appendChild(failedElem);
    }

    if (manualAudits.length) {
      const manualEl = this._renderManualAudits(manualAudits, category.manualDescription);
      element.appendChild(manualEl);
    }

    if (passedElements.length) {
      const passedElem = this.renderPassedAuditsSection(passedElements);
      element.appendChild(passedElem);
    }

    if (notApplicableElements.length) {
      const notApplicableElem = this._renderNotApplicableAuditsSection(notApplicableElements);
      element.appendChild(notApplicableElem);
    }

    return element;
  }

  /**
   * Create a non-semantic span used for hash navigation of categories
   * @param {Element} element
   * @param {string} id
   */
  createPermalinkSpan(element, id) {
    const permalinkEl = this.dom.createChildOf(element, 'span', 'lh-permalink');
    permalinkEl.id = id;
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = CategoryRenderer;
} else {
  self.CategoryRenderer = CategoryRenderer;
}
