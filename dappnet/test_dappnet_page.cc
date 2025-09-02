// Simple test to verify dappnet page loads
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class DappnetPageTest : public InProcessBrowserTest {
 public:
  DappnetPageTest() = default;
};

IN_PROC_BROWSER_TEST_F(DappnetPageTest, LoadDappnetPage) {
  // Test chrome://dappnet/
  GURL dappnet_url("chrome://dappnet/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dappnet_url));
  
  content::WebContents* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check the URL actually loaded (not redirected to error page)
  GURL final_url = web_contents->GetLastCommittedURL();
  EXPECT_EQ(dappnet_url, final_url) << "Expected: " << dappnet_url << " Got: " << final_url;
}

IN_PROC_BROWSER_TEST_F(DappnetPageTest, LoadDappnetConfigPage) {
  // Test chrome://dappnet/config
  GURL config_url("chrome://dappnet/config");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), config_url));
  
  content::WebContents* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check the URL actually loaded
  GURL final_url = web_contents->GetLastCommittedURL();
  EXPECT_EQ(config_url, final_url) << "Expected: " << config_url << " Got: " << final_url;
}