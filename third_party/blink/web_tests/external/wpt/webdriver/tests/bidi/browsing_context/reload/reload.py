from pathlib import Path

import pytest


pytestmark = pytest.mark.asyncio

PNG_BLACK_DOT = "/webdriver/tests/bidi/browsing_context/support/black_dot.png"


async def test_return_value(bidi_session, inline, new_tab):
    url = inline("<div>foo</div>")
    await bidi_session.browsing_context.navigate(context=new_tab["context"],
                                                 url=url,
                                                 wait="complete")

    result = await bidi_session.browsing_context.reload(
        context=new_tab["context"],
        wait="complete"
    )
    assert result == {}


@pytest.mark.parametrize("hash", [False, True], ids=["without hash", "with hash"])
async def test_reload(bidi_session, inline, new_tab, hash):
    target_url = inline("""<div id="foo""")
    if hash:
        target_url += "#foo"

    await bidi_session.browsing_context.navigate(context=new_tab["context"],
                                                 url=target_url,
                                                 wait="complete")
    await bidi_session.browsing_context.reload(context=new_tab["context"],
                                               wait="complete")

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab['context'])
    assert len(contexts) == 1
    assert contexts[0]["url"] == target_url


@pytest.mark.parametrize(
    "url",
    [
        "about:blank",
        "data:text/html,<p>foo</p>",
        f'{Path(__file__).parents[1].as_uri()}/support/empty.html',
    ],
    ids=[
        "about:blank",
        "data url",
        "file url",
    ],
)
async def test_reload_special_protocols(bidi_session, new_tab, url):
    await bidi_session.browsing_context.navigate(context=new_tab["context"],
                                                 url=url,
                                                 wait="complete")
    await bidi_session.browsing_context.reload(context=new_tab["context"],
                                               wait="complete")

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab['context'])
    assert len(contexts) == 1
    assert contexts[0]["url"] == url


async def test_image(bidi_session, new_tab, url):
    initial_url = url(PNG_BLACK_DOT)
    await bidi_session.browsing_context.navigate(context=new_tab["context"],
                                                 url=initial_url,
                                                 wait="complete")
    await bidi_session.browsing_context.reload(context=new_tab["context"],
                                               wait="complete")

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab['context'])
    assert len(contexts) == 1
    assert contexts[0]["url"] == initial_url
