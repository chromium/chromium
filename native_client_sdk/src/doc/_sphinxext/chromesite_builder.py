# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# This is a Sphinx extension.
#

from __future__ import print_function
import codecs
from collections import namedtuple, OrderedDict
import os
import string
from docutils import nodes
from docutils.parsers.rst import Directive, directives
from sphinx.util.osutil import ensuredir
from sphinx.builders.html import StandaloneHTMLBuilder
from sphinx.writers.html import HTMLWriter
from sphinx.writers.html import SmartyPantsHTMLTranslator as HTMLTranslator
from sphinx.util.console import bold

# PEPPER_VERSION = "31"

# TODO(eliben): it may be interesting to use an actual Sphinx template here at
# some point.
PAGE_TEMPLATE = string.Template(r'''
{{+bindTo:partials.${doc_template}}}

${doc_body}

{{/partials.${doc_template}}}
'''.lstrip())


# Path to the top-level YAML table-of-contents file for the chromesite
BOOK_TOC_TEMPLATE = '_book_template.yaml'


class ChromesiteHTMLTranslator(HTMLTranslator):
  """ Custom HTML translator for chromesite output.

      Hooked into the HTML builder by setting the html_translator_class
      option in conf.py

      HTMLTranslator is provided by Sphinx. We're actually using
      SmartyPantsHTMLTranslator to use its quote and dash-formatting
      capabilities. It's a subclass of the HTMLTranslator provided by docutils,
      with Sphinx-specific features added. Here we provide chromesite-specific
      behavior by overriding some of the visiting methods.
  """
  def __init__(self, builder, *args, **kwds):
    # HTMLTranslator is an old-style Python class, so 'super' doesn't work: use
    # direct parent invocation.
    HTMLTranslator.__init__(self, builder, *args, **kwds)

    self.within_toc = False

  def visit_bullet_list(self, node):
    # Use our own class attribute for <ul>. Don't care about compacted lists.
    self.body.append(self.starttag(node, 'ul', **{'class': 'small-gap'}))

  def depart_bullet_list(self, node):
    # Override to not pop anything from context
    self.body.append('</ul>\n')

  def visit_literal(self, node):
    # Don't insert "smart" quotes here
    self.no_smarty += 1
    # Sphinx emits <tt></tt> for literals (``like this``), with <span> per word
    # to protect against wrapping, etc. We're required to emit plain <code>
    # tags for them.
    # Emit a simple <code> tag without enabling "protect_literal_text" mode,
    # so Sphinx's visit_Text doesn't mess with the contents.
    self.body.append(self.starttag(node, 'code', suffix=''))

  def depart_literal(self, node):
    self.no_smarty -= 1
    self.body.append('</code>')

  def visit_literal_block(self, node):
    # Don't insert "smart" quotes here
    self.no_smarty += 1
    # We don't use Sphinx's buildin pygments integration for code highlighting,
    # because the chromesite requires special <pre> tags for that and handles
    # the highlighting on its own.
    attrs = {'class': 'prettyprint'} if node.get('prettyprint', 1) else {}
    self.body.append(self.starttag(node, 'pre', **attrs))

  def depart_literal_block(self, node):
    self.no_smarty -= 1
    self.body.append('\n</pre>\n')

  def visit_title(self, node):
    if isinstance(node.parent, nodes.section):
      # Steal the id from the parent. This is used in chromesite to handle the
      # auto-generated navbar and permalinks.
      if node.parent.hasattr('ids'):
        node['ids'] = node.parent['ids'][:]

    HTMLTranslator.visit_title(self, node)


  def visit_section(self, node):
    # chromesite needs <section> instead of <div class='section'>
    self.section_level += 1
    if self.section_level == 1:
      self.body.append(self.starttag(node, 'section'))

  def depart_section(self, node):
    if self.section_level == 1:
      self.body.append('</section>')
    self.section_level -= 1

  def visit_image(self, node):
    # Paths to images in .rst sources should be absolute. This visitor does the
    # required transformation for the path to be correct in the final HTML.
    # if self.builder.chromesite_production_mode:
    node['uri'] = self.builder.get_production_url(node['uri'])
    HTMLTranslator.visit_image(self, node)

  def visit_reference(self, node):
    # In "kill_internal_links" mode, we don't emit the actual links for internal
    # nodes.
    if self.builder.chromesite_kill_internal_links and node.get('internal'):
      pass
    else:
      HTMLTranslator.visit_reference(self, node)

  def depart_reference(self, node):
    if self.builder.chromesite_kill_internal_links and node.get('internal'):
      pass
    else:
      HTMLTranslator.depart_reference(self, node)

  def visit_topic(self, node):
    if 'contents' in node['classes']:
      # TODO(binji):
      # Detect a TOC: we want to hide these from chromesite, but still keep
      # them in devsite. An easy hack is to add display: none to the element
      # here.
      # When we remove devsite support, we can remove this hack.
      self.within_toc = True
      attrs = {'style': 'display: none'}
      self.body.append(self.starttag(node, 'div', **attrs))
    else:
      HTMLTranslator.visit_topic(self, node)

  def depart_topic(self, node):
    if self.within_toc:
      self.body.append('\n</div>')
    else:
      HTMLTranslator.visit_topic(self, node)

  def write_colspecs(self):
    # Override this method from docutils to do nothing. We don't need those
    # pesky <col width=NN /> tags in our markup.
    pass

  def visit_admonition(self, node, name=''):
    self.body.append(self.starttag(node, 'aside', CLASS=node.get('class', '')))

  def depart_admonition(self, node=''):
    self.body.append('\n</aside>\n')

  def unknown_visit(self, node):
    raise NotImplementedError('Unknown node: ' + node.__class__.__name__)


class ChromesiteBuilder(StandaloneHTMLBuilder):
  """ Builder for the NaCl chromesite HTML output.

      Loosely based on the code of Sphinx's standard SerializingHTMLBuilder.
  """
  name = 'chromesite'
  out_suffix = '.html'
  link_suffix = '.html'

  # Disable the addition of "pi"-permalinks to each section header
  add_permalinks = False

  def init(self):
    self.config.html_translator_class = \
        'chromesite_builder.ChromesiteHTMLTranslator'
    self.chromesite_kill_internal_links = \
        int(self.config.chromesite_kill_internal_links) == 1
    self.info("----> Chromesite builder")
    self.config_hash = ''
    self.tags_hash = ''
    self.theme = None       # no theme necessary
    self.templates = None   # no template bridge necessary
    self.init_translator_class()
    self.init_highlighter()

  def finish(self):
    super(ChromesiteBuilder, self).finish()
    # if self.chromesite_production_mode:
    #   # We decided to keep the manual _book.yaml for now;
    #   # The code for auto-generating YAML TOCs from index.rst was removed in
    #   # https://codereview.chromium.org/57923006/
    #   self.info(bold('generating YAML table-of-contents... '))
    #   subs = { 'version': PEPPER_VERSION }
    #   with open(os.path.join(self.env.srcdir, '_book.yaml')) as in_f:
    #     with open(os.path.join(self.outdir, '_book.yaml'), 'w') as out_f:
    #       out_f.write(string.Template(in_f.read()).substitute(subs))
    self.info()

  def dump_inventory(self):
    # We don't want an inventory file when building for chromesite
    # if not self.chromesite_production_mode:
    #   super(ChromesiteBuilder, self).dump_inventory()
    pass

  def get_production_url(self, url):
    # if not self.chromesite_production_mode:
    #   return url

    return '/native-client/%s' % url

  def get_target_uri(self, docname, typ=None):
    # if self.chromesite_production_mode:
      return self.get_production_url(docname) + self.link_suffix
    # else:
    #   return docname + self.link_suffix

  def handle_page(self, pagename, ctx, templatename='page.html',
                  outfilename=None, event_arg=None):
    ctx['current_page_name'] = pagename

    if not outfilename:
      outfilename = os.path.join(self.outdir,
                                 pagename + self.out_suffix)

    # Emit an event to Sphinx
    self.app.emit('html-page-context', pagename, templatename,
                  ctx, event_arg)

    ensuredir(os.path.dirname(outfilename))
    self._dump_context(ctx, outfilename)

  def _dump_context(self, context, filename):
    """ Do the actual dumping of the page to the file. context is a dict. Some
        important fields:
          body - document contents
          title
          current_page_name
        Some special pages (genindex, etc.) may not have some of the fields, so
        fetch them conservatively.
    """
    if not 'body' in context:
      return

    template = context.get('meta', {}).get('template', 'standard_nacl_article')
    title = context.get('title', '')
    body = context.get('body', '')

    # codecs.open is the fast Python 2.x way of emulating the encoding= argument
    # in Python 3's builtin open.
    with codecs.open(filename, 'w', encoding='utf-8') as f:
      f.write(PAGE_TEMPLATE.substitute(
        doc_template=template,
        doc_title=title,
        doc_body=body))

  def _conditional_chromesite(self, s):
    # return s if self.chromesite_production_mode else ''
    return s

  def _conditional_nonprod(self, s):
    # return s if not self.chromesite_production_mode else ''
    return ''


class NaclCodeDirective(Directive):
  """ Custom "naclcode" directive for code snippets. To keep it under our
      control.
  """
  has_content = True
  required_arguments = 0
  optional_arguments = 1
  option_spec = {
      'prettyprint': int,
  }

  def run(self):
    code = u'\n'.join(self.content)
    literal = nodes.literal_block(code, code)
    literal['prettyprint'] = self.options.get('prettyprint', 1)
    return [literal]

def setup(app):
  """ Extension registration hook.
  """
  # linkcheck issues HEAD requests to save time, but some Google properties
  # reject them and we get spurious 405 responses. Monkey-patch sphinx to
  # just use normal GET requests.
  # See: https://bitbucket.org/birkenfeld/sphinx/issue/1292/
  from sphinx.builders import linkcheck
  import urllib2
  linkcheck.HeadRequest = urllib2.Request

  app.add_directive('naclcode', NaclCodeDirective)
  app.add_builder(ChromesiteBuilder)

  # "Production mode" for local testing vs. on-server documentation.
  app.add_config_value('chromesite_kill_internal_links', default='0',
                       rebuild='html')
