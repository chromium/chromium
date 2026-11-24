import pytest
import webdriver.bidi.error as error

pytestmark = pytest.mark.asyncio


async def test_uninstall(bidi_session, extension_data):
    # Prefer "base64" so remote browsers (e.g. Android) do not depend on a
    # host filesystem path, falling back to "path" when "base64" is
    # unsupported.
    try:
        web_extension = await bidi_session.web_extension.install(
            extension_data={
                "type": "base64",
                "value": extension_data["base64"],
            }
        )
    except error.UnsupportedOperationException:
        web_extension = await bidi_session.web_extension.install(
            extension_data={
                "type": "path",
                "path": extension_data["path"],
            }
        )
    await bidi_session.web_extension.uninstall(
        extension=web_extension
    )
    # proof that the uninstall was successful
    with pytest.raises(error.NoSuchWebExtensionException):
        await bidi_session.web_extension.uninstall(
            extension=web_extension
        )
