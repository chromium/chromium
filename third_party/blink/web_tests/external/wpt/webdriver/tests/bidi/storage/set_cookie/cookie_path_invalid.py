import pytest
from webdriver.bidi.modules.network import NetworkStringValue
from webdriver.bidi.modules.storage import PartialCookie, BrowsingContextPartitionDescriptor
from .. import assert_cookie_is_set
import webdriver.bidi.error as error

pytestmark = pytest.mark.asyncio

COOKIE_NAME = 'SOME_COOKIE_NAME'
COOKIE_VALUE = 'SOME_COOKIE_VALUE'


@pytest.mark.parametrize(
    "path",
    [
        ""
        "no_leading_forward_slash"
    ]
)
async def test_path_invalid_values(bidi_session, top_context, test_page, origin, domain_value, path):
    # Navigate to a secure context.
    await bidi_session.browsing_context.navigate(context=top_context["context"], url=test_page, wait="complete")

    partition = BrowsingContextPartitionDescriptor(top_context["context"])

    with pytest.raises(error.UnableToSetCookieException):
        await bidi_session.storage.set_cookie(
            cookie=PartialCookie(
                name=COOKIE_NAME,
                value=NetworkStringValue(COOKIE_VALUE),
                domain=domain_value(),
                path=path,
                secure=True
            ),
            partition=partition)
